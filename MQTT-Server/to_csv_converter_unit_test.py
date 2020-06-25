import unittest
from data_access import DataAccess
from to_csv_converter import CsvConverter
import os


class ToCsvConverterTestCase(unittest.TestCase):
    def setUp(self):
        db_address = 'localhost'
        db_port = 27017
        db_name = 'apc-iot'
        db_coll_name = 'apc_data'
        self.db_access = DataAccess(db_address, db_port, db_coll_name)
        self.db_access.connect_db(db_name)
        self.assertTrue(self.db_access.is_connection_active)
        self.csv_filename = 'test.csv'

    def test_should_parse_correct_inputs(self):
        columns_sensor = ['Temperature (°C)', 'Humidity (H)', 'PM25 (ug/m3)', 'PM25 (PPM)', 'CO (PPM)', 'O3 (PPB)',
                          'Wind Speed (m/s)', 'Wind Direction']
        columns_external = ['weather']
        invalid_values = ['', 0, -1]

        converter = CsvConverter(self.db_access, columns_sensor, columns_external, invalid_values,
                                 self.csv_filename)

        converter.convert()

    def test_should_parse_correct_inputs_with_no_invalids(self):
        columns_sensor = ['Temperature (°C)', 'Humidity (H)', 'PM25 (ug/m3)', 'PM25 (PPM)', 'CO (PPM)', 'O3 (PPB)',
                          'Wind Speed (m/s)', 'Wind Direction']
        columns_external = ['weather']
        invalid_values = []

        converter = CsvConverter(self.db_access, columns_sensor, columns_external, invalid_values,
                                 self.csv_filename)

        converter.convert()

    def test_should_parse_correct_inputs_dummy(self):
        columns_sensor = ['address']
        invalid_values = ['', 0, -1]

        converter = CsvConverter(self.db_access, columns_sensor, [], invalid_values,
                                 self.csv_filename)

        converter.convert()

    def tearDown(self):
        os.remove(self.csv_filename)


if __name__ == '__main__':
    unittest.main()
