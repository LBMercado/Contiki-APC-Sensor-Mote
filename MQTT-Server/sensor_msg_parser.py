import json
from sensor_translator import SensorUnit, SensorType, SensorTranslator, \
    switch_sensor_string_type_to_type, switch_sensor_type_to_unit_string, switch_sensor_type_to_unit
from sensor_message import SensorMessage
from typing import List


class SensorMessageParser:
    def __init__(self, json_str_messages: List[str]):
        self.contents = []
        self.sensor_msgs: List[SensorMessage] = []
        for msg in json_str_messages:
            try:
                self.contents.append(json.loads(msg))
            except json.JSONDecodeError:
                print('ERROR: SensorMessage constructor parameter \'json_str_message\' is not formatted correctly.')
                continue
            sensor_msg = SensorMessage()
            # Get collector info
            sensor_msg.collector_info = self.contents[-1]["collector_info"]
            # Get collector data
            sensor_msg.collector_data = self.contents[-1]['collector_sensor_data']
            # Make calibration values into collector info
            sensor_msg.collector_info['calibration'] = sensor_msg.collector_data.pop('calibration')

            self.sensor_msgs.append(sensor_msg)

    def unload_messages(self, json_str_messages: List[str]):
        self.contents = []
        self.sensor_msgs: List[SensorMessage] = []
        for msg in json_str_messages:
            try:
                self.contents.append(json.loads(msg))
            except json.JSONDecodeError:
                print('ERROR: SensorMessage constructor parameter \'json_str_message\' is not formatted correctly.')
                continue
            sensor_msg = SensorMessage()
            # Get collector info
            sensor_msg.collector_info = self.contents[-1]["collector_info"]

            # Get collector data
            sensor_msg.collector_data = self.contents[-1]['collector_sensor_data']
            # Make calibration values into collector info
            sensor_msg.collector_info['calibration'] = sensor_msg.collector_data.pop('calibration')

            self.sensor_msgs.append(sensor_msg)

    def parse_data(self):
        translator = SensorTranslator()

        for msg in self.sensor_msgs:
            collector_data = {}
            for k, v in msg.collector_data.items():
                sensor_string_type = k.split('(')[0].rstrip()
                sensor_type = switch_sensor_string_type_to_type(sensor_string_type)
                if sensor_type is not None and v > 0:
                    new_key = sensor_string_type + " (" + switch_sensor_type_to_unit_string(sensor_type) + ")"

                    translator.type = sensor_type
                    translator.unit = switch_sensor_type_to_unit(sensor_type)
                    translator.set_raw_value(v)
                    new_value = translator.translate()

                    collector_data[new_key] = round(new_value, 3)
                else:
                    collector_data[k] = v
            msg.collector_data = collector_data
