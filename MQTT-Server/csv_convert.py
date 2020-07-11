from data_access import DataAccess
from to_csv_converter import CsvConverter

''' DB Configuration '''
DB_ADDRESS = 'localhost'
DB_PORT = 27017
DB_NAME = 'apc-iot'
DB_COLL_NAME = 'apc_data'
''' CSV Configuration '''
CSV_FILENAME = 'air_quality_data.csv'

def main():
    db_access = DataAccess(DB_ADDRESS, DB_PORT, DB_COLL_NAME)
    db_access.connect_db(DB_NAME)

    columns_sensor = ['Temperature (°C)', 'Humidity (%RH)', 'PM25 (ug/m3)',
                      'CO (PPM)', 'NO2 (PPM)', 'O3 (PPB)',
                      'Wind Speed (m/s)', 'Wind Direction']
    columns_external = ['weather', 'date']
    invalid_values = ['', -1]

    converter = CsvConverter(db_access, columns_sensor, columns_external, invalid_values,
                             CSV_FILENAME)

    converter.convert()


if __name__ == '__main__':
    main()
