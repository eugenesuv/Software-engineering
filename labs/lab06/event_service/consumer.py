from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from typing import Any

import pika

from common import QUEUE_NAME, declare_topology, wait_for_rabbitmq


STATE_PATH = Path(os.getenv("CQRS_STATE_PATH", "/state/read_model.json"))
MAX_MESSAGES = int(os.getenv("CONSUMER_MAX_MESSAGES", "0"))


def empty_state() -> dict[str, Any]:
    return {
        "processedEventIds": [],
        "rentals": {},
        "notifications": [],
    }


def load_state() -> dict[str, Any]:
    if not STATE_PATH.exists():
        return empty_state()

    with STATE_PATH.open("r", encoding="utf-8") as file:
        state = json.load(file)

    state.setdefault("processedEventIds", [])
    state.setdefault("rentals", {})
    state.setdefault("notifications", [])
    return state


def save_state(state: dict[str, Any]) -> None:
    STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = STATE_PATH.with_suffix(".tmp")
    with tmp_path.open("w", encoding="utf-8") as file:
        json.dump(state, file, ensure_ascii=False, indent=2, sort_keys=True)
        file.write("\n")
    tmp_path.replace(STATE_PATH)


def handle_event(envelope: dict[str, Any]) -> bool:
    state = load_state()
    processed_ids = set(state["processedEventIds"])
    event_id = str(envelope["eventId"])

    if event_id in processed_ids:
        print(f"duplicate ignored eventId={event_id}", flush=True)
        return False

    event_type = str(envelope["eventType"])
    payload = envelope.get("payload") or {}
    rental_id = str(payload.get("rentalId") or envelope.get("aggregate", {}).get("id") or "")
    if not rental_id:
        raise ValueError("Rental event has no rentalId")

    rental = dict(state["rentals"].get(rental_id) or {})
    rental.update(
        {
            "rentalId": rental_id,
            "userId": payload.get("userId", rental.get("userId")),
            "carId": payload.get("carId", rental.get("carId")),
            "status": payload.get("status", rental.get("status")),
            "lastEventId": event_id,
            "updatedAt": envelope.get("occurredAt"),
        }
    )

    if event_type == "RentalCreated":
        rental["status"] = payload.get("status", "ACTIVE")
        notification_kind = "rental_created"
    elif event_type == "RentalCompleted":
        rental["status"] = payload.get("status", "COMPLETED")
        notification_kind = "rental_completed"
    else:
        raise ValueError(f"Unsupported event type for this consumer: {event_type}")

    state["rentals"][rental_id] = rental
    state["notifications"].append(
        {
            "eventId": event_id,
            "kind": notification_kind,
            "rentalId": rental_id,
            "userId": rental.get("userId"),
            "createdAt": envelope.get("occurredAt"),
        }
    )
    state["notifications"] = state["notifications"][-100:]
    state["processedEventIds"].append(event_id)

    save_state(state)
    print(
        f"processed eventId={event_id} type={event_type} rentalId={rental_id} status={rental['status']}",
        flush=True,
    )
    return True


def main() -> int:
    connection = wait_for_rabbitmq()
    channel = connection.channel()
    declare_topology(channel)
    channel.basic_qos(prefetch_count=10)

    processed_messages = 0

    def on_message(
        ch: pika.adapters.blocking_connection.BlockingChannel,
        method: pika.spec.Basic.Deliver,
        properties: pika.BasicProperties,
        body: bytes,
    ) -> None:
        nonlocal processed_messages
        try:
            envelope = json.loads(body.decode("utf-8"))
            handle_event(envelope)
            ch.basic_ack(delivery_tag=method.delivery_tag)
            processed_messages += 1

            if MAX_MESSAGES and processed_messages >= MAX_MESSAGES:
                ch.stop_consuming()
        except Exception as exc:
            print(
                f"consumer failed messageId={getattr(properties, 'message_id', '')}: {exc}",
                file=sys.stderr,
                flush=True,
            )
            ch.basic_nack(delivery_tag=method.delivery_tag, requeue=False)

    print(f"waiting for messages queue={QUEUE_NAME} state={STATE_PATH}", flush=True)
    channel.basic_consume(queue=QUEUE_NAME, on_message_callback=on_message, auto_ack=False)

    try:
        channel.start_consuming()
    finally:
        if connection.is_open:
            connection.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
