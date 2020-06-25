from data_access import DataAccess
from sensor_aggregator import SensorAggregator
import csv


class CsvConverter:
    def __init__(self, db_access: DataAccess, columns_sensor: list, columns_external: list,
                 invalid_values: list, csv_filename: str):
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

    def convert(self):
        if self.columns_sensor.count == 0:
            raise ValueError("columns_sensor is empty.")

        filter_column = {'collectors': {'$elemMatch': {'$or': [{key: {}} for key in self.columns_sensor]}}}

        # make sure field exists in document and it does not have invalid values
        for or_clause in filter_column['collectors']['$elemMatch']['$or']:
            for key in or_clause.keys():
                if len(self.invalid_values) > 0:
                    or_clause[key] = {'$exists': True, '$nin': self.invalid_values}
                else:
                    or_clause[key] = {'$exists': True}

        for column in self.columns_external:
            if len(self.invalid_values) > 0:
                filter_column[column] = {'$exists': True, '$nin': self.invalid_values}
            else:
                filter_column[column] = {'$exists': True}
        print('filter set: ' + str(filter_column))

        documents = self.db_access.get_documents(filter_column)
        print('found {} data from collection \'{}\''.format(len(documents), self.db_access.collection_name))

        # remove unneeded key pairs
        for document in documents:
            keys_to_remove = []
            inner_keys_to_remove = []
            for key in document:
                if key == 'collectors':
                    for collector in document['collectors']:
                        for inner_key in collector.keys():
                            if inner_key not in self.columns_sensor and inner_key not in self.columns_external:
                                inner_keys_to_remove.append(inner_key)
                elif key not in self.columns_sensor and key not in self.columns_external:
                    keys_to_remove.append(key)
            for key in keys_to_remove:
                document.pop(key)
            for inner_key in inner_keys_to_remove:
                for collector in document['collectors']:
                    collector.pop(inner_key)

        # normalize for csv reader
        for i in range(len(documents)):
            aggregator = SensorAggregator(documents[i]['collectors'], self.tolerance)
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
