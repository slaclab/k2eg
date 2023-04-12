from kafka import KafkaProducer

class Producer:
    def __init__(self, kafka_server):
        self.producer = KafkaProducer(bootstrap_servers=[kafka_server])

    def send(self, topic:str, message: str):
        self.producer.send(
            topic,
            message
        )
        self.producer.flush()
    def close(self):
        self.producer.close()