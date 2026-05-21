from __future__ import annotations

import os
import sys
import time

import pika
from pymongo import MongoClient

from common import EXCHANGE_NAME, declare_topology, encode_json, make_event_envelope, routing_key_for_event, wait_for_rabbitmq


MONGO_URL = os.getenv("MONGO_URL", "mongodb://mongo:27017/?replicaSet=rs0")
MONGO_DB_NAME = os.getenv("MONGO_DB_NAME", "car_rental")
OUTBOX_COLLECTION = os.getenv("OUTBOX_COLLECTION", "outbox_events")
BATCH_LIMIT = int(os.getenv("PRODUCER_BATCH_LIMIT", "100"))


def load_outbox_events() -> list[dict]:
    client = MongoClient(MONGO_URL, serverSelectionTimeoutMS=10_000)
    client.admin.command("ping")
    database = client[MONGO_DB_NAME]

    cursor = (
        database[OUTBOX_COLLECTION]
        .find({})
        .sort([("createdAt", 1), ("_id", 1)])
        .limit(BATCH_LIMIT)
    )
    events = list(cursor)

    rentals = database["rentals"]
    for event in events:
        payload = dict(event.get("payload") or {})
        if payload.get("userId") and payload.get("carId"):
            continue

        rental_id = str(event.get("aggregateId") or payload.get("rentalId") or "")
        if not rental_id:
            continue

        rental = rentals.find_one({"_id": rental_id}, {"userId": 1, "carId": 1})
        if not rental:
            continue

        payload.setdefault("userId", rental.get("userId"))
        payload.setdefault("carId", rental.get("carId"))
        event["payload"] = payload

    return events


def publish_events(events: list[dict]) -> int:
    connection = wait_for_rabbitmq()
    try:
        channel = connection.channel()
        declare_topology(channel)
        channel.confirm_delivery()

        published = 0
        for outbox_event in events:
            envelope = make_event_envelope(outbox_event)
            routing_key = routing_key_for_event(envelope["eventType"])
            body = encode_json(envelope)

            channel.basic_publish(
                exchange=EXCHANGE_NAME,
                routing_key=routing_key,
                body=body,
                mandatory=True,
                properties=pika.BasicProperties(
                    content_type="application/json",
                    delivery_mode=2,
                    message_id=envelope["eventId"],
                    type=envelope["eventType"],
                    timestamp=int(time.time()),
                    headers={
                        "eventType": envelope["eventType"],
                        "eventVersion": envelope["eventVersion"],
                        "aggregateType": envelope["aggregate"]["type"],
                    },
                ),
            )
            published += 1
            print(
                f"published eventId={envelope['eventId']} type={envelope['eventType']} routingKey={routing_key}",
                flush=True,
            )

        return published
    finally:
        connection.close()


def main() -> int:
    events = load_outbox_events()
    if not events:
        print("outbox is empty; nothing to publish", flush=True)
        return 0

    published = publish_events(events)
    print(f"published_total={published}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"producer failed: {exc}", file=sys.stderr, flush=True)
        raise
