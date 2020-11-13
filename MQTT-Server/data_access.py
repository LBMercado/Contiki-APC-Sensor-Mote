from datetime import datetime
from pymongo import MongoClient, DESCENDING as pymongo_DESCENDING
from sensor_aggregator import SensorAggregator
from typing import Union

class DataAccess:
    def __init__(self, address, port: int, db_name: str = ''):
        try:
            if not address or not port or port < 1:
                raise ValueError("Invalid address or port specified.")
            self.address = address
            self.port = port
            self.db_name = db_name
            if self.db_name:
                self.client = MongoClient(address, port)
                self.connect_db(db_name)
            else:
                self.is_connection_active = False
                self.cur_db = None
                self.client = MongoClient(address, port)
        except ValueError as err:
            print(err)

    def connect_db(self, db_name: str):
        if not db_name:
            print("Invalid database name specified.")
            return
        self.db_name = db_name
        self.cur_db = self.client[db_name]
        self.is_connection_active = True

    def disconnect_db(self):
        self.db_name = ''
        self.cur_db = None
        self.is_connection_active = False

    def insert_document(self, document: dict, coll_name: str):
        if not self.is_connection_active or self.cur_db is None:
            print("Failed to insert document: No active database connection.")
            return
        if not coll_name or not document:
            print("Failed to insert document: collection_name or document is null.")
            return
        collection = self.cur_db[coll_name]
        result = collection.insert_one(document)
        print("Inserted in collection \'{}\' document with id: {}".format(coll_name, result.inserted_id))

    def get_documents(self, criteria: dict, coll_name: str):
        documents = []
        for document in self.cur_db[coll_name].find(criteria):
            documents.append(document)
        return documents

    def get_first_document(self, criteria: dict, coll_name: str, projection: dict = None):
        # will get latest document first
        if projection:
            return self.cur_db[coll_name].find(criteria, projection, sort=[("date", pymongo_DESCENDING)]).limit(1)[0]
        else:
            return self.cur_db[coll_name].find(criteria, sort=[("date", pymongo_DESCENDING)]).limit(1)[0]

    def get_first_documents(self, criteria: dict, coll_name: str, limit: int, projection: dict = None):
        # will get latest document first
        cursor = None
        documents = []
        if projection:
            cursor = self.cur_db[coll_name].find(criteria, projection,
                                                 sort=[("date", pymongo_DESCENDING)]).limit(limit)
        else:
            cursor = self.cur_db[coll_name].find(criteria, sort=[("date", pymongo_DESCENDING)]).limit(limit)
        for document in cursor:
            documents.append(document)
        return documents

    def delete_documents(self, criteria: dict, coll_name: str) -> int:
        delete_count = self.cur_db[coll_name].delete_many(criteria)
        return delete_count.deleted_count


class ApcDataAccess(DataAccess):
    def __init__(self, address, port: int, db_name: str, mote_data_coll_name: str, pred_data_coll_name: str):
        super().__init__(address, port, db_name)
        self.mote_data_coll_name = mote_data_coll_name
        self.pred_data_coll_name = pred_data_coll_name

    def get_all_sensor_data_normalized(self, columns_sensor: list, columns_external: list, invalid_values: list,
                                       before_date: datetime, after_date: Union[datetime, None], tolerance: float):
        filter_column = {'$or': []}
        filter_column['$or'] \
            .append({'collectors_data': {'$elemMatch': {'$or': [{key: {}} for key in columns_sensor]}}})

        # make sure at least one valid field exists in document
        #   sensor data
        for or_clause in filter_column['$or'][0]['collectors_data']['$elemMatch']['$or']:
            for key in or_clause.keys():
                or_clause[key] = {'$exists': True}

        #   external data
        for column in columns_external:
            filter_column['$or'].append({column: {'$exists': True}})
            if column == 'date':
                # check if dates are provided and specify query restriction, otherwise ignore
                #       get all data later than the after_date specified
                if after_date is not None and before_date is None:
                    filter_column['date'] = {'$gte': after_date}
                #       get data between the two dates specified
                elif after_date is not None and before_date is not None:
                    filter_column['date'] = {'$gte': after_date, '$lte': before_date}
                #       get all data prior to the before_date specified
                elif after_date is None and before_date is not None:
                    filter_column['date'] = {'$lte': before_date}

        print('filter set: ' + str(filter_column))

        documents = self.get_documents(filter_column, self.mote_data_coll_name)
        print('found {} data from collection \'{}\''.format(len(documents), self.mote_data_coll_name))

        # remove unneeded key pairs
        for document in documents:
            for key in list(document.keys()):
                if key == 'collectors_data':
                    for collector in document['collectors_data']:
                        for inner_key, inner_value in list(collector.items()):
                            if inner_key not in columns_sensor \
                                    or inner_value in invalid_values:
                                del collector[inner_key]

                elif key not in columns_external:
                    del document[key]

        # normalize sensor data values
        for i in range(len(documents)):
            aggregator = SensorAggregator(documents[i]['collectors_data'], tolerance)
            aggregator.aggregate()
            avg_sensor_data = aggregator.average_sensor_data

            # add in the external fields
            for key in documents[i]:
                if key in columns_external:
                    avg_sensor_data[key] = documents[i][key]
            documents[i] = avg_sensor_data

            # pad empty fields
            column_list = [*columns_sensor, *columns_external]
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

        return documents

    def get_limit_sensor_data_normalized(self, columns_sensor: list, columns_external: list, invalid_values: list,
                                         before_date: datetime, after_date: Union[datetime, None], tolerance: float, limit: int):
        filter_column = {'$or': []}
        filter_column['$or'] \
            .append({'collectors_data': {'$elemMatch': {'$or': [{key: {}} for key in columns_sensor]}}})

        # make sure at least one valid field exists in document
        #   sensor data
        for or_clause in filter_column['$or'][0]['collectors_data']['$elemMatch']['$or']:
            for key in or_clause.keys():
                or_clause[key] = {'$exists': True}

        #   external data
        for column in columns_external:
            filter_column['$or'].append({column: {'$exists': True}})
            if column == 'date':
                # check if dates are provided and specify query restriction, otherwise ignore
                #       get all data later than the after_date specified
                if after_date is not None and before_date is None:
                    filter_column['date'] = {'$gte': after_date}
                #       get data between the two dates specified
                elif after_date is not None and before_date is not None:
                    filter_column['date'] = {'$gte': after_date, '$lte': before_date}
                #       get all data prior to the before_date specified
                elif after_date is None and before_date is not None:
                    filter_column['date'] = {'$lte': before_date}

        print('filter set: ' + str(filter_column))

        documents = self.get_first_documents(filter_column, self.mote_data_coll_name, limit)
        print('found {} data from collection \'{}\''.format(len(documents), self.mote_data_coll_name))

        # remove unneeded key pairs
        for document in documents:
            for key in list(document.keys()):
                if key == 'collectors_data':
                    for collector in document['collectors_data']:
                        for inner_key, inner_value in list(collector.items()):
                            if inner_key not in columns_sensor \
                                    or inner_value in invalid_values:
                                del collector[inner_key]

                elif key not in columns_external:
                    del document[key]

        # normalize sensor data values
        for i in range(len(documents)):
            aggregator = SensorAggregator(documents[i]['collectors_data'], tolerance)
            aggregator.aggregate()
            avg_sensor_data = aggregator.average_sensor_data

            # add in the external fields
            for key in documents[i]:
                if key in columns_external:
                    avg_sensor_data[key] = documents[i][key]
            documents[i] = avg_sensor_data

            # pad empty fields
            column_list = [*columns_sensor, *columns_external]
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

        return documents

    def get_latest_sensor_data_normalized(self, columns_sensor: list, columns_external: list, invalid_values: list,
                                          before_date: datetime, after_date: Union[datetime, None], tolerance: float):
        filter_column = {'$or': []}
        filter_column['$or'] \
            .append({'collectors_data': {'$elemMatch': {'$or': [{key: {}} for key in columns_sensor]}}})

        # make sure at least one valid field exists in document
        #   sensor data
        for or_clause in filter_column['$or'][0]['collectors_data']['$elemMatch']['$or']:
            for key in or_clause.keys():
                or_clause[key] = {'$exists': True}

        #   external data
        for column in columns_external:
            filter_column['$or'].append({column: {'$exists': True}})
            if column == 'date':
                # check if dates are provided and specify query restriction, otherwise ignore
                #       get all data later than the after_date specified
                if after_date is not None and before_date is None:
                    filter_column['date'] = {'$gte': after_date}
                #       get data between the two dates specified
                elif after_date is not None and before_date is not None:
                    filter_column['date'] = {'$gte': after_date, '$lte': before_date}
                #       get all data prior to the before_date specified
                elif after_date is None and before_date is not None:
                    filter_column['date'] = {'$lte': before_date}

        print('filter set: ' + str(filter_column))

        document = self.get_first_document(filter_column, self.mote_data_coll_name)

        if document is None or document == {}:
            return None

        # remove unneeded key pairs
        for key in list(document.keys()):
            if key == 'collectors_data':
                for collector in document['collectors_data']:
                    for inner_key, inner_value in list(collector.items()):
                        if inner_key not in columns_sensor \
                                or inner_value in invalid_values:
                            del collector[inner_key]

            elif key not in columns_external:
                del document[key]

        # normalize sensor data values
        aggregator = SensorAggregator(document['collectors_data'], tolerance)
        aggregator.aggregate()
        avg_sensor_data = aggregator.average_sensor_data

        # add in the external fields
        for key in document:
            if key in columns_external:
                avg_sensor_data[key] = document[key]
        document = avg_sensor_data

        # pad empty fields
        column_list = [*columns_sensor, *columns_external]
        for column in column_list:
            if column not in document.keys():
                document[column] = ''

        # stringify array fields
        for key, val in document.items():
            if isinstance(val, list):
                new_val = str(val).replace(',', '&')
                # remove non-alphanumeric characters
                new_val = ''.join(char for char in new_val if char.isalnum() or char == '&')
                document[key] = new_val

        return document

    def get_mote_info(self, columns_include: list):
        filter_column = {'$or': []}
        filter_column['$or'] \
            .append({'collectors_info': {'$elemMatch': {'$or': [{key: {}} for key in columns_include]}}})

        # make sure at least one valid field exists in document
        for or_clause in filter_column['$or'][0]['collectors_info']['$elemMatch']['$or']:
            for key in or_clause.keys():
                or_clause[key] = {'$exists': True}

        # get latest document
        filter_column['date'] = {'$lte': datetime.now()}

        print('filter set: ' + str(filter_column))

        document = self.get_first_document(filter_column, self.mote_data_coll_name)

        # remove unneeded key pairs
        for key in list(document.keys()):
            if key == 'collectors_info':
                for collector in document['collectors_info']:
                    for inner_key, inner_value in list(collector.items()):
                        if inner_key not in columns_include:
                            del collector[inner_key]

            else:
                del document[key]

        document = document['collectors_info']
        return document

    def get_limit_predictions(self, count: int, base_date: Union[datetime, None] = None):
        filters = {}
        if base_date is None:
            filters = {'date': {'$gte': datetime.now()}}
        else:
            filters = {'date': {'$gte': base_date}}
        projection = {'_id': False}
        preds = self.get_first_documents(filters, self.pred_data_coll_name, count, projection)
        preds.reverse()
        return preds

