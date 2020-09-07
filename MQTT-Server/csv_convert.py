from data_access import DataAccess
from to_csv_converter import CsvConverter
import datetime
from configparser import ConfigParser

''' CSV Configuration '''
CSV_PATH = 'csv_data/air_quality_data_sample.csv'


def convert_csv(file_name: str):
    config = ConfigParser()
    config.read('config.ini')
    db_name = config['mongodb']['db_name']
    db_address = config['mongodb']['db_address']
    db_port = int(config['mongodb']['db_port'])
    db_collection = config['mongodb']['db_collection']

    db_access = DataAccess(db_address, db_port, db_name, db_collection)
    db_access.connect_db(db_name)

    columns_sensor = config['csv_api']['columns_sensor'].split(',')
    columns_sensor = [x.strip() for x in columns_sensor]
    columns_external = config['csv_api']['columns_external'].split(',')
    columns_external = [x.strip() for x in columns_external]
    invalid_values = config['csv_api']['invalid_values'].split(',')
    invalid_values = [x.strip() for x in invalid_values]
    for i in range(len(invalid_values)):
        if _is_number(invalid_values[i]):
            invalid_values[i] = int(invalid_values[i])

    start_date = datetime.datetime(2020, 7, 13, 17, 23, 56)
    end_date = datetime.datetime(2020, 7, 13, 18, 23, 54)

    converter = CsvConverter(db_access, columns_sensor, columns_external, invalid_values,
                             file_name, after_date=start_date, before_date=end_date)

    converter.convert()


def main():
    convert_csv(CSV_PATH)


# helper function for invalid_values
def _is_number(num_string: str):
    try:
        f = float(num_string)
        return True
    except ValueError:
        return False


if __name__ == '__main__':
    main()
