from data_access import DataAccess
import csv
import datetime
from db_helper import db_get_normalized


class CsvConverter:
    def __init__(self, db_access: DataAccess, columns_sensor: list, columns_external: list,
                 invalid_values: list, csv_filename: str, after_date: datetime = None, before_date: datetime = None):
        self.db_access = db_access
        if not self.db_access.is_connection_active:
            raise ValueError("no connection to database.")
        self.columns_sensor = columns_sensor
        self.columns_external = columns_external
        self.invalid_values = invalid_values
        self.csv_filename = csv_filename
        if self.columns_sensor.count == 0:
            raise ValueError("columns_sensor is empty.")
        self.tolerance = 10
        self.before_date = before_date
        self.after_date = after_date



    def convert(self):
        if self.columns_sensor.count == 0:
            raise ValueError("columns_sensor is empty.")

        documents = db_get_normalized(self.columns_sensor, self.columns_external, self.invalid_values,
                                      self.before_date, self.after_date, self.tolerance, self.db_access)

        if len(documents) == 0:
            print('WARNING - no data to write other than headers.')
        print('writing into file \'{}\'...'.format(self.csv_filename))
        with open(self.csv_filename, 'w') as csv_file:
            csv_columns = [*self.columns_sensor, *self.columns_external]

            writer = csv.DictWriter(csv_file, fieldnames=csv_columns)

            writer.writeheader()
            # write data
            for document in documents:
                writer.writerow(document)
        print('write done!')
