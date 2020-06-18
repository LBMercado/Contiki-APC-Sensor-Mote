from pymongo import MongoClient


class DataAccess:
    def __init__(self, address, port):
        try:
            if not address or not port or port < 1:
                raise ValueError("Invalid address or port specified.")
            self.address = address
            self.port = port
            self.is_connection_active = False
            self.cur_db = None
            self.client = MongoClient(address, port)
        except ValueError as err:
            print(err)

    def connect_db(self, db_name):
        if not db_name:
            print("Invalid database name specified.")
            return
        self.cur_db = self.client[db_name]
        self.is_connection_active = True

    def disconnect_db(self):
        self.cur_db = None
        self.is_connection_active = False

    def insert_document(self, record, document):
        if not self.is_connection_active or self.cur_db is None:
            print("Failed to insert document: No active database connection.")
            return
        if not record or not document:
            print("Failed to insert document: record or document is null.")
            return
        record = self.cur_db[record]
        result = record.insert_one(document)
        print("Inserted document with id: {0}".format(result.inserted_id))
