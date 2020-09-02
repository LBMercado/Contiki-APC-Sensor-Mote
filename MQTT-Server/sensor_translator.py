from enum import Enum, unique
import math


@unique
class SensorType(Enum):
    MQ7_TYPE = 0
    MQ131_TYPE = 1
    MQ135_TYPE = 2
    MICS4514_RED_TYPE = 3
    MICS4514_NOX_TYPE = 4


@unique
class SensorUnit(Enum):
    PPM = 0
    PPB = 1
    MILLIGRAM_PER_CUBIC_METER = 2
    MICROGRAM_PER_CUBIC_METER = 3


class SensorTranslator:
    def __init__(self, sensor_type: SensorType = SensorType.MQ7_TYPE, sensor_unit: SensorUnit = SensorUnit.PPM):
        self.type = sensor_type
        self.unit = sensor_unit
        self._raw_value = 0.0

    def set_raw_value(self, raw_value: float):
        if raw_value > 0:
            self._raw_value = raw_value
        else:
            raise ValueError('Argument \'raw_value\' must be nonzero and non-negative')

    def translate(self):
        if self.type == SensorType.MQ7_TYPE:
            return _SwitchTranslator.mq7_translate(self._raw_value, self.unit)
        elif self.type == SensorType.MQ131_TYPE:
            return _SwitchTranslator.mq131_translate(self._raw_value, self.unit)
        elif self.type == SensorType.MQ135_TYPE:
            return _SwitchTranslator.mq135_translate(self._raw_value, self.unit)
        elif self.type == SensorType.MICS4514_RED_TYPE:
            return _SwitchTranslator.mics4514_red_translate(self._raw_value, self.unit)
        elif self.type == SensorType.MICS4514_NOX_TYPE:
            return _SwitchTranslator.mics4514_nox_translate(self._raw_value, self.unit)
        else:
            raise ValueError('@SensorTranslator function translate: invalid sensor type specified.')


class _SwitchTranslator:
    RESRATIO_TOLERANCE = 0.0001
    # PPM/PPB REFERENCE VALUES
    # reference datasheet approximate values for MQ7 PPM (CO)
    MQ7_RESRATIO_MIN = 0.09
    MQ7_RESRATIO_MAX = 1.75
    MQ7_RESRATIO_BOUNDARIES = (MQ7_RESRATIO_MAX, 1.0, 0.4, 0.225, MQ7_RESRATIO_MIN)
    MQ7_PPM_MIN = 50.0
    MQ7_PPM_MAX = 4000.0
    MQ7_PPM_BOUNDARIES = (MQ7_PPM_MIN, 100.0, 400.0, 1000, MQ7_PPM_MAX)
    # reference datasheet approximate values for MQ131 PPB (O3)
    MQ131_RESRATIO_MIN = 1.25
    MQ131_RESRATIO_MAX = 8
    MQ131_RESRATIO_BOUNDARIES = (MQ131_RESRATIO_MIN, 2, 2.75, 4, 6, MQ131_RESRATIO_MAX)
    MQ131_PPB_MIN = 10
    MQ131_PPB_MAX = 1000
    MQ131_PPB_BOUNDARIES = (MQ131_PPB_MIN, 50, 100, 200, 500, MQ131_PPB_MAX)
    # reference datasheet approximate values for MQ135 PPM (CO2)
    MQ135_RESRATIO_MIN = 0.8
    MQ135_RESRATIO_MAX = 2.5
    MQ135_RESRATIO_BOUNDARIES = (MQ135_RESRATIO_MAX, 1.1, MQ135_RESRATIO_MIN)
    MQ135_PPM_MIN = 10.0
    MQ135_PPM_MAX = 200.0
    MQ135_PPM_BOUNDARIES = (MQ135_PPM_MIN, 100.0, MQ135_PPM_MAX)
    # reference datasheet approximate values for MICS4514-RED PPM (CO)
    MICS4514_RED_RESRATIO_MIN = 0.01
    MICS4514_RED_RESRATIO_MAX = 3.5
    MICS4514_RED_RESRATIO_BOUNDARIES = (MICS4514_RED_RESRATIO_MAX, MICS4514_RED_RESRATIO_MIN)
    MICS4514_RED_PPM_MIN = 0.9
    MICS4514_RED_PPM_MAX = 1000
    MICS4514_RED_PPM_BOUNDARIES = (MICS4514_RED_PPM_MIN, MICS4514_RED_PPM_MAX)
    # reference datasheet approximate values for MICS4514-NOX PPM (NO2)
    MICS4514_NOX_RESRATIO_MIN = 0.065
    MICS4514_NOX_RESRATIO_MAX = 40.0
    MICS4514_NOX_RESRATIO_BOUNDARIES = (MICS4514_NOX_RESRATIO_MAX, MICS4514_NOX_RESRATIO_MIN)
    MICS4514_NOX_PPM_MIN = 0.01
    MICS4514_NOX_PPM_MAX = 6.0
    MICS4514_NOX_PPM_BOUNDARIES = (MICS4514_NOX_PPM_MIN, MICS4514_NOX_PPM_MAX)

    @staticmethod
    def mq7_translate(raw_value: float, unit: SensorUnit = SensorUnit.PPM):
        # Make sure valid is within range of sensor specs else set to min/max and warn user
        if raw_value + _SwitchTranslator.RESRATIO_TOLERANCE < _SwitchTranslator.MQ7_RESRATIO_MIN:
            print('mq7_translate: WARNING - raw_value of value {} out of bounds, minimum value {} set.'
                  .format(raw_value, _SwitchTranslator.MQ7_RESRATIO_MIN))
            raw_value = _SwitchTranslator.MQ7_RESRATIO_MIN
        elif raw_value - _SwitchTranslator.RESRATIO_TOLERANCE > _SwitchTranslator.MQ7_RESRATIO_MAX:
            print('mq7_translate: WARNING - raw_value of value {} out of bounds, maximum value {} set.'
                  .format(raw_value, _SwitchTranslator.MQ7_RESRATIO_MAX))
            raw_value = _SwitchTranslator.MQ7_RESRATIO_MAX
        # Compare from the top since the boundaries are in descending order
        boundary_len = len(_SwitchTranslator.MQ7_RESRATIO_BOUNDARIES)
        boundary_max_index = boundary_len - 1
        for index in range(1, boundary_len):
            if raw_value - _SwitchTranslator.RESRATIO_TOLERANCE <= \
                    _SwitchTranslator.MQ7_RESRATIO_BOUNDARIES[boundary_max_index - index]:
                computed_value = _SwitchTranslator._get_log_log_function(
                    x_comp_0=_SwitchTranslator.MQ7_RESRATIO_BOUNDARIES[boundary_max_index - index],
                    x_comp_1=_SwitchTranslator.MQ7_RESRATIO_BOUNDARIES[boundary_max_index - index + 1],
                    y_comp_0=_SwitchTranslator.MQ7_PPM_BOUNDARIES[boundary_max_index - index],
                    y_comp_1=_SwitchTranslator.MQ7_PPM_BOUNDARIES[boundary_max_index - index + 1]
                )(raw_value)

                return _SwitchTranslator._switch_units(computed_value, SensorUnit.PPM, unit)
        raise ValueError('function mq7_translate: raw_value is out of bounds.')

    @staticmethod
    def mq131_translate(raw_value: float, unit: SensorUnit = SensorUnit.PPB):
        # Make sure valid is within range of sensor specs else set to min/max and warn user
        if raw_value + _SwitchTranslator.RESRATIO_TOLERANCE < _SwitchTranslator.MQ131_RESRATIO_MIN:
            print('mq131_translate: WARNING - raw_value of value {} out of bounds, minimum value {} set.'
                  .format(raw_value, _SwitchTranslator.MQ131_RESRATIO_MIN))
            raw_value = _SwitchTranslator.MQ131_RESRATIO_MIN
        elif raw_value - _SwitchTranslator.RESRATIO_TOLERANCE > _SwitchTranslator.MQ131_RESRATIO_MAX:
            print('mq131_translate: WARNING - raw_value of value {} out of bounds, maximum value {} set.'
                  .format(raw_value, _SwitchTranslator.MQ131_RESRATIO_MAX))
            raw_value = _SwitchTranslator.MQ131_RESRATIO_MAX
        # Compare from the bottom since the boundaries are in ascending order
        boundary_len = len(_SwitchTranslator.MQ131_RESRATIO_BOUNDARIES)
        for index in range(1, boundary_len - 1):
            if raw_value - _SwitchTranslator.RESRATIO_TOLERANCE <= \
                    _SwitchTranslator.MQ131_RESRATIO_BOUNDARIES[index]:
                computed_value = _SwitchTranslator._get_log_log_function(
                    x_comp_0=_SwitchTranslator.MQ131_RESRATIO_BOUNDARIES[index - 1],
                    x_comp_1=_SwitchTranslator.MQ131_RESRATIO_BOUNDARIES[index],
                    y_comp_0=_SwitchTranslator.MQ131_PPB_BOUNDARIES[index - 1],
                    y_comp_1=_SwitchTranslator.MQ131_PPB_BOUNDARIES[index]
                )(raw_value)

                return _SwitchTranslator._switch_units(computed_value, SensorUnit.PPB, unit)
        raise ValueError('function mq131_translate: raw_value is out of bounds.')

    @staticmethod
    def mq135_translate(raw_value: float, unit: SensorUnit = SensorUnit.PPM):
        # Make sure valid is within range of sensor specs else set to min/max and warn user
        if raw_value + _SwitchTranslator.RESRATIO_TOLERANCE < _SwitchTranslator.MQ135_RESRATIO_MIN:
            print('mq135_translate: WARNING - raw_value of value {} out of bounds, minimum value {} set.'
                  .format(raw_value, _SwitchTranslator.MQ135_RESRATIO_MIN))
            raw_value = _SwitchTranslator.MQ135_RESRATIO_MIN
        elif raw_value - _SwitchTranslator.RESRATIO_TOLERANCE > _SwitchTranslator.MQ135_RESRATIO_MAX:
            print('mq135_translate: WARNING - raw_value of value {} out of bounds, maximum value {} set.'
                  .format(raw_value, _SwitchTranslator.MQ135_RESRATIO_MAX))
            raw_value = _SwitchTranslator.MQ135_RESRATIO_MAX
        # Compare from the top since the boundaries are in descending order
        boundary_len = len(_SwitchTranslator.MQ135_RESRATIO_BOUNDARIES)
        boundary_max_index = boundary_len - 1
        for index in range(1, boundary_len):
            if raw_value - _SwitchTranslator.RESRATIO_TOLERANCE <= \
                    _SwitchTranslator.MQ135_RESRATIO_BOUNDARIES[boundary_max_index - index]:
                computed_value = _SwitchTranslator._get_log_log_function(
                    x_comp_0=_SwitchTranslator.MQ135_RESRATIO_BOUNDARIES[boundary_max_index - index],
                    x_comp_1=_SwitchTranslator.MQ135_RESRATIO_BOUNDARIES[boundary_max_index - index + 1],
                    y_comp_0=_SwitchTranslator.MQ135_PPM_BOUNDARIES[boundary_max_index - index],
                    y_comp_1=_SwitchTranslator.MQ135_PPM_BOUNDARIES[boundary_max_index - index + 1]
                )(raw_value)

                return _SwitchTranslator._switch_units(computed_value, SensorUnit.PPM, unit)
        raise ValueError('function mq135_translate: raw_value is out of bounds.')

    @staticmethod
    def mics4514_red_translate(raw_value: float, unit: SensorUnit = SensorUnit.PPM):
        # Make sure valid is within range of sensor specs else set to min/max and warn user
        if raw_value + _SwitchTranslator.RESRATIO_TOLERANCE < _SwitchTranslator.MICS4514_RED_RESRATIO_MIN:
            print('mics4514_red_translate: WARNING - raw_value of value {} out of bounds, minimum value {} set.'
                  .format(raw_value, _SwitchTranslator.MICS4514_RED_RESRATIO_MIN))
            raw_value = _SwitchTranslator.MICS4514_RED_RESRATIO_MIN
        elif raw_value - _SwitchTranslator.RESRATIO_TOLERANCE > _SwitchTranslator.MICS4514_RED_RESRATIO_MAX:
            print('mics4514_red_translate: WARNING - raw_value of value {} out of bounds, maximum value {} set.'
                  .format(raw_value, _SwitchTranslator.MICS4514_RED_RESRATIO_MAX))
            raw_value = _SwitchTranslator.MICS4514_RED_RESRATIO_MAX

        computed_value = _SwitchTranslator._get_log_log_function(
            x_comp_0=_SwitchTranslator.MICS4514_RED_RESRATIO_BOUNDARIES[0],
            x_comp_1=_SwitchTranslator.MICS4514_RED_RESRATIO_BOUNDARIES[1],
            y_comp_0=_SwitchTranslator.MICS4514_RED_PPM_BOUNDARIES[0],
            y_comp_1=_SwitchTranslator.MICS4514_RED_PPM_BOUNDARIES[1]
        )(raw_value)

        return _SwitchTranslator._switch_units(computed_value, SensorUnit.PPM, unit)

    @staticmethod
    def mics4514_nox_translate(raw_value: float, unit: SensorUnit = SensorUnit.PPM):
        # Make sure valid is within range of sensor specs else set to min/max and warn user
        if raw_value + _SwitchTranslator.RESRATIO_TOLERANCE < _SwitchTranslator.MICS4514_NOX_RESRATIO_MIN:
            print('mics4514_nox_translate: WARNING - raw_value of value {} out of bounds, minimum value {} set.'
                  .format(raw_value, _SwitchTranslator.MICS4514_NOX_RESRATIO_MIN))
            raw_value = _SwitchTranslator.MICS4514_NOX_RESRATIO_MIN
        elif raw_value - _SwitchTranslator.RESRATIO_TOLERANCE > _SwitchTranslator.MICS4514_NOX_RESRATIO_MAX:
            print('mics4514_nox_translate: WARNING - raw_value of value {} out of bounds, maximum value {} set.'
                  .format(raw_value, _SwitchTranslator.MICS4514_NOX_RESRATIO_MAX))
            raw_value = _SwitchTranslator.MICS4514_NOX_RESRATIO_MAX

        computed_value = _SwitchTranslator._get_log_log_function(
            x_comp_0=_SwitchTranslator.MICS4514_NOX_RESRATIO_BOUNDARIES[0],
            x_comp_1=_SwitchTranslator.MICS4514_NOX_RESRATIO_BOUNDARIES[1],
            y_comp_0=_SwitchTranslator.MICS4514_NOX_PPM_BOUNDARIES[0],
            y_comp_1=_SwitchTranslator.MICS4514_NOX_PPM_BOUNDARIES[1]
        )(raw_value)

        return _SwitchTranslator._switch_units(computed_value, SensorUnit.PPM, unit)

    @staticmethod
    def _switch_units(value: float, from_unit: SensorUnit, to_unit: SensorUnit):
        if from_unit == SensorUnit.PPM:
            ppm_selector = \
                {
                    SensorUnit.PPM: value,
                    SensorUnit.PPB: value * 1000,
                    SensorUnit.MILLIGRAM_PER_CUBIC_METER: value,
                    SensorUnit.MICROGRAM_PER_CUBIC_METER: value * 1000
                }
            return ppm_selector[to_unit]
        elif from_unit == SensorUnit.PPB:
            ppb_selector = \
                {
                    SensorUnit.PPM: value / 1000,
                    SensorUnit.PPB: value,
                    SensorUnit.MILLIGRAM_PER_CUBIC_METER: value * 1000,
                    SensorUnit.MICROGRAM_PER_CUBIC_METER: value
                }
            return ppb_selector[to_unit]
        elif from_unit == SensorUnit.MILLIGRAM_PER_CUBIC_METER:
            milligram_selector = \
                {
                    SensorUnit.PPM: value,
                    SensorUnit.PPB: value / 1000,
                    SensorUnit.MILLIGRAM_PER_CUBIC_METER: value,
                    SensorUnit.MICROGRAM_PER_CUBIC_METER: value * 1000
                }
            return milligram_selector[to_unit]
        elif from_unit == SensorUnit.MICROGRAM_PER_CUBIC_METER:
            microgram_selector = \
                {
                    SensorUnit.PPM: value / 1000,
                    SensorUnit.PPB: value,
                    SensorUnit.MILLIGRAM_PER_CUBIC_METER: value * 1000,
                    SensorUnit.MICROGRAM_PER_CUBIC_METER: value
                }
            return microgram_selector[to_unit]
        else:
            raise ValueError('function _switch_units: invalid from_unit argument. ')

    @staticmethod
    def _get_log_log_function(x_comp_0: float, x_comp_1: float, y_comp_0: float, y_comp_1: float):
        return lambda value: y_comp_0 * (value / x_comp_0) ** (math.log(y_comp_1 / y_comp_0) /
                                                               math.log(x_comp_1 / x_comp_0))


# take note that the specified gases can be represented by other sensors,
# make changes depending on your application
def switch_sensor_string_type_to_type(sensor_string):
    switcher = {
        "NO2": SensorType.MICS4514_NOX_TYPE,
        "CO": SensorType.MICS4514_RED_TYPE,
        "O3": SensorType.MQ131_TYPE
    }
    return switcher.get(sensor_string, None)


def switch_sensor_type_to_unit_string(sensor_type: SensorType):
    switcher = {
        SensorType.MICS4514_NOX_TYPE: "PPM",
        SensorType.MICS4514_RED_TYPE: "PPM",
        SensorType.MQ131_TYPE: "PPB"
    }
    return switcher.get(sensor_type, None)

def switch_sensor_type_to_unit(sensor_type: SensorType):
    switcher = {
        SensorType.MICS4514_NOX_TYPE: SensorUnit.PPM,
        SensorType.MICS4514_RED_TYPE: SensorUnit.PPM,
        SensorType.MQ131_TYPE: SensorUnit.PPB
    }
    return switcher.get(sensor_type, None)