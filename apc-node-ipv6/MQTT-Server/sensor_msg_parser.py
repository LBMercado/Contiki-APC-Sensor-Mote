import json
from sensor_translator import SensorUnit, SensorType, SensorTranslator, \
    switch_sensor_string_type_to_type, switch_sensor_type_to_unit_string, switch_sensor_type_to_unit


class SensorMessage:
    def __init__(self, json_str_message: str):
        try:
            self.content = json.loads(json_str_message)
        except json.JSONDecodeError:
            print('ERROR: SensorMessage constructor parameter \'json_str_message\' is not formatted correctly.')
            self.content = {}
        # Get sink info
        self.sink = self.content["sink"]

        # Get collector info
        self.collectors = [v for k, v in self.content.items() if k.startswith('collector')]

    def unload_message(self, json_str_message: str):
        try:
            self.content = json.loads(json_str_message)
        except json.JSONDecodeError:
            print('ERROR: SensorMessage constructor parameter \'json_str_message\' is not formatted correctly.')
            self.content = {}
        # Get sink info
        self.sink = self.content["sink"]

        # Get collector info
        self.collectors = [v for k, v in self.content.items() if k.startswith('collector')]

    def parse_data(self):
        translator = SensorTranslator()
        collectors_copy = []

        for collector in self.collectors:
            collector_copy = {}

            for k, v in collector.items():
                sensor_string_type = k.split('(')[0].rstrip()
                sensor_type = switch_sensor_string_type_to_type(sensor_string_type)
                if sensor_type is not None and v > 0:
                    new_key = sensor_string_type + " (" + switch_sensor_type_to_unit_string(sensor_type) + ")"

                    translator.type = sensor_type
                    translator.unit = switch_sensor_type_to_unit(sensor_type)
                    translator.set_raw_value(v)
                    new_value = translator.translate()

                    collector_copy[new_key] = round(new_value, 3)
                else:
                    collector_copy[k] = v

            collectors_copy.append(collector_copy)

        self.collectors = collectors_copy
