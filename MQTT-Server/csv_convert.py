from data_access import DataAccess
from to_csv_converter import CsvConverter
import datetime
from configparser import ConfigParser

''' DB Configuration '''
DB_ADDRESS = 'localhost'
DB_PORT = 27017
DB_NAME = 'apc-iot'
DB_COLL_NAME = 'apc_data'
''' CSV Configuration '''
CSV_FILENAME = 'air_quality_data.csv'


def main():
    db_access = DataAccess(DB_ADDRESS, DB_PORT, DB_NAME, DB_COLL_NAME)
    db_access.connect_db(DB_NAME)

    config = ConfigParser()
    config.read('config.ini')

    columns_sensor = config['csv_api']['columns_sensor'].split(',')
    columns_sensor = [x.strip() for x in columns_sensor]
    columns_external = config['csv_api']['columns_external'].split(',')
    columns_external = [x.strip() for x in columns_external]
    invalid_values = config['csv_api']['invalid_values'].split(',')
    invalid_values = [x.strip() for x in invalid_values]
    for i in range(len(invalid_values)):
        if is_number(invalid_values[i]):
            invalid_values[i] = int(invalid_values[i])

    start_date = datetime.datetime(2020, 7, 13, 17, 23, 56)
    end_date = datetime.datetime(2020, 7, 13, 18, 23, 54)

    converter = CsvConverter(db_access, columns_sensor, columns_external, invalid_values,
                             CSV_FILENAME, after_date=start_date, before_date=end_date)

    converter.convert()


# helper function for invalid_values
def is_number(num_string: str):
    try:
        f = float(num_string)
        return True
    except ValueError:
        return False


if __name__ == '__main__':
    main()
