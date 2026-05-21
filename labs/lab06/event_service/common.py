from __future__ import annotations

import datetime as dt
import json
import os
import time
from typing import Any, Optional

import pika


EXCHANGE_NAME = os.getenv("RABBITMQ_EXCHANGE", "car_rental.events")
DLX_NAME = os.getenv("RABBITMQ_DLX", "car_rental.dlx")
QUEUE_NAME = os.getenv("RABBITMQ_QUEUE", "notification.rental-events")
DLQ_NAME = os.getenv("RABBITMQ_DLQ", "notification.rental-events.dlq")
RABBITMQ_URL = os.getenv("RABBITMQ_URL", "amqp://guest:guest@rabbitmq:5672/%2F")

EVENT_ROUTING_KEYS = {
    "RentalCreated": "rental.created",
    "RentalCompleted": "rental.completed",
    "UserRegistered": "user.registered",
    "CarAdded": "car.added",
    "CarStatusChanged": "car.status_changed",
}


def wait_for_rabbitmq(url: str = RABBITMQ_URL, attempts: int = 30, delay_seconds: float = 2.0) -> pika.BlockingConnection:
    parameters = pika.URLParameters(url)
    last_error: Optional[Exception] = None

    for attempt in range(1, attempts + 1):
        try:
            return pika.BlockingConnection(parameters)
        except pika.exceptions.AMQPConnectionError as exc:
            last_error = exc
            print(f"RabbitMQ is not ready yet ({attempt}/{attempts}); retrying in {delay_seconds:g}s", flush=True)
            time.sleep(delay_seconds)

    raise RuntimeError("RabbitMQ connection failed") from last_error


def declare_topology(channel: pika.adapters.blocking_connection.BlockingChannel) -> None:
    channel.exchange_declare(exchange=EXCHANGE_NAME, exchange_type="topic", durable=True)
    channel.exchange_declare(exchange=DLX_NAME, exchange_type="direct", durable=True)
    channel.queue_declare(queue=DLQ_NAME, durable=True)
    channel.queue_bind(queue=DLQ_NAME, exchange=DLX_NAME, routing_key=DLQ_NAME)

    channel.queue_declare(
        queue=QUEUE_NAME,
        durable=True,
        arguments={
            "x-dead-letter-exchange": DLX_NAME,
            "x-dead-letter-routing-key": DLQ_NAME,
        },
    )
    channel.queue_bind(queue=QUEUE_NAME, exchange=EXCHANGE_NAME, routing_key="rental.*")


def routing_key_for_event(event_type: str) -> str:
    try:
        return EVENT_ROUTING_KEYS[event_type]
    except KeyError as exc:
        raise ValueError(f"Unsupported event type: {event_type}") from exc


def normalize_timestamp(value: Any) -> str:
    if isinstance(value, dt.datetime):
        if value.tzinfo is None:
            value = value.replace(tzinfo=dt.timezone.utc)
        return value.astimezone(dt.timezone.utc).isoformat().replace("+00:00", "Z")

    if isinstance(value, str) and value:
        return value

    return dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00", "Z")


def json_default(value: Any) -> str:
    if isinstance(value, dt.datetime):
        return normalize_timestamp(value)
    return str(value)


def make_event_envelope(outbox_event: dict[str, Any]) -> dict[str, Any]:
    payload = dict(outbox_event.get("payload") or {})
    event_type = str(outbox_event["eventType"])
    outbox_id = str(outbox_event["_id"])
    aggregate_type = str(outbox_event.get("aggregateType", "rental"))
    aggregate_id = str(outbox_event.get("aggregateId") or payload.get("rentalId", ""))
    event_id = str(payload.get("eventId") or outbox_id)
    occurred_at = normalize_timestamp(outbox_event.get("createdAt") or payload.get("createdAt"))

    return {
        "eventId": event_id,
        "eventType": event_type,
        "eventVersion": 1,
        "occurredAt": occurred_at,
        "producer": f"{aggregate_type}-service",
        "aggregate": {
            "type": aggregate_type,
            "id": aggregate_id,
        },
        "payload": payload,
        "metadata": {
            "outboxId": outbox_id,
            "source": "rental_service.outbox_events",
            "delivery": "at-least-once",
        },
    }


def encode_json(data: dict[str, Any]) -> bytes:
    return json.dumps(data, ensure_ascii=False, sort_keys=True, default=json_default).encode("utf-8")
