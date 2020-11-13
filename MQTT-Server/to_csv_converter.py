import os
import csv
import datetime
from io import StringIO
from flask import stream_with_context
from data_access import ApcDataAccess


class CsvConverter:
    def __init__(self, db_access: ApcDataAccess, columns_sensor: list, columns_external: list,
                 invalid_values: list, csv_path: str, after_date: datetime = None, before_date: datetime = None):
        self.db_access = db_access
        if not self.db_access.is_connection_active:
            raise ValueError("no connection to database.")
        self.columns_sensor = columns_sensor
        self.columns_external = columns_external
        self.invalid_values = invalid_values
        self.csv_path = csv_path
        if self.columns_sensor.count == 0:
            raise ValueError("columns_sensor is empty.")
        self.tolerance = 10
        self.before_date = before_date
        self.after_date = after_date

    def convert(self):
        if self.columns_sensor.count == 0:
            raise ValueError("columns_sensor is empty.")

        documents = self.db_access.get_all_sensor_data_normalized(self.columns_sensor, self.columns_external,
                                                                  self.invalid_values, self.before_date,
                                                                  self.after_date, self.tolerance)

        if len(documents) == 0:
            print('WARNING - no data to write other than headers.')
        if not os.path.isfile(self.csv_path):
            os.makedirs(os.path.dirname(self.csv_path), exist_ok=True)
        print('writing into path \'{}\'...'.format(self.csv_path))
        with open(self.csv_path, 'w') as csv_file:
            csv_columns = [*self.columns_sensor, *self.columns_external]

            writer = csv.DictWriter(csv_file, fieldnames=csv_columns)

            writer.writeheader()
            # write data
            for document in documents:
                writer.writerow(document)
        print('write done!')

    @stream_with_context
    def stream_convert(self):
        if self.columns_sensor.count == 0:
            raise ValueError("columns_sensor is empty.")

        documents = self.db_access.get_all_sensor_data_normalized(self.columns_sensor, self.columns_external,
                                                                  self.invalid_values,self.before_date,
                                                                  self.after_date, self.tolerance)

        if len(documents) == 0:
            print('WARNING - no data to write other than headers.')
        csv_columns = [*self.columns_sensor, *self.columns_external]
        data_stream = StringIO()
        writer = csv.DictWriter(data_stream, fieldnames=csv_columns)
        print('writing csv into stream...')

        # write header
        writer.writeheader()
        yield data_stream.getvalue()
        data_stream.seek(0)
        data_stream.truncate(0)

        # write each row item
        for document in documents:
            writer.writerow(document)
            yield data_stream.getvalue()
            data_stream.seek(0)
            data_stream.truncate(0)

        print('write done!')
