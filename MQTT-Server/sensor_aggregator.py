from typing import List, Dict


# This class expects normalized sensors_data fields
class SensorAggregator:
    def __init__(self, sensors_data: List[Dict], tolerance: float = 0.001):
        self.sensors_data = sensors_data
        self.average_sensor_data = {}
        self.tolerance = tolerance

    def aggregate(self):
        if len(self.sensors_data) > 1:
            sum_count = {}

            for sensor_data in self.sensors_data:
                for key in sensor_data:
                    if key not in self.average_sensor_data:
                        self.average_sensor_data[key] = sensor_data[key]
                        if not isinstance(sensor_data[key], str):
                            sum_count[key] = 1
                    elif not isinstance(sensor_data[key], str) \
                            and (self.average_sensor_data[key] / sum_count[key]) - sensor_data[key] <= self.tolerance:
                        self.average_sensor_data[key] += sensor_data[key]
                        sum_count[key] += 1
                    elif isinstance(sensor_data[key], str) and self.average_sensor_data[key] == sensor_data[key]:
                        # we ignore inconsistent str values, but we store the first we find
                        pass
                    else:
                        print('WARNING - inconsistent sensor_data value for type \'{}\''.format(key))
                        print('Base Value: {}'.format(self.average_sensor_data[key]))
                        print('Compared Value: {}'.format(sensor_data[key]))

            # average the values
            for key, factor in sum_count.items():
                self.average_sensor_data[key] /= factor
        else:
            self.average_sensor_data = self.sensors_data[0]
