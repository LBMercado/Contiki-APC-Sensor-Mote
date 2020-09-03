from data_access import DataAccess
from sensor_aggregator import SensorAggregator
import csv
import datetime


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

        filter_column = {'$or': []}
        filter_column['$or']\
            .append({'collectors_data': {'$elemMatch': {'$or': [{key: {}} for key in self.columns_sensor]}}})

        # make sure at least one valid field exists in document
        #   sensor data
        for or_clause in filter_column['$or'][0]['collectors_data']['$elemMatch']['$or']:
            for key in or_clause.keys():
                or_clause[key] = {'$exists': True}

        #   external data
        for column in self.columns_external:
            filter_column['$or'].append({column: {'$exists': True}})
            if column == 'date':
                # check if dates are provided and specify query restriction, otherwise ignore
                #       get all data later than the after_date specified
                if self.after_date is not None and self.before_date is None:
                    filter_column['date'] = {'$gte': self.after_date}
                #       get data between the two dates specified
                elif self.after_date is not None and self.before_date is not None:
                    filter_column['date'] = {'$gte': self.after_date, '$lte': self.before_date}
                #       get all data prior to the before_date specified
                elif self.after_date is None and self.before_date is not None:
                    filter_column['date'] = {'$lte': self.before_date}

        print('filter set: ' + str(filter_column))

        documents = self.db_access.get_documents(filter_column)
        print('found {} data from collection \'{}\''.format(len(documents), self.db_access.collection_name))

        # remove unneeded key pairs
        for document in documents:
            for key in list(document.keys()):
                if key == 'collectors_data':
                    for collector in document['collectors_data']:
                        for inner_key, inner_value in list(collector.items()):
                            if inner_key not in self.columns_sensor \
                                    or inner_value in self.invalid_values:
                                del collector[inner_key]

                elif key not in self.columns_external:
                    del document[key]

        # normalize for csv reader
        for i in range(len(documents)):
            aggregator = SensorAggregator(documents[i]['collectors_data'], self.tolerance)
            aggregator.aggregate()
            avg_sensor_data = aggregator.average_sensor_data

            # add in the external fields
            for key in documents[i]:
                if key in self.columns_external:
                    avg_sensor_data[key] = documents[i][key]
            documents[i] = avg_sensor_data

            # pad empty fields
            column_list = [*self.columns_sensor, *self.columns_external]
            for column in column_list:
                if column not in documents[i].keys():
                    documents[i][column] = ''

            # stringify array fields
            for key, val in documents[i].items():
                if isinstance(val, list):
                    new_val = str(val).replace(',', '&')
                    # remove non-alphanumeric characters
                    new_val = ''.join(char for char in new_val if char.isalnum() or char == '&')
                    documents[i][key] = new_val

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
