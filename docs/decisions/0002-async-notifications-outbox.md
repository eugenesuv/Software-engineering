# 2. Asynchronous notifications via broker and outbox
Date: 2026-03-09

## Status
Accepted

## Context
Sending email/SMS inside `POST /rentals` request path increases latency and couples booking flow to provider availability.  
At the same time, rental events must not be lost.

## Decision
Use asynchronous event delivery:
- `Rental Service` publishes `RentalCreated` and `RentalCompleted` events to Message Broker.
- Publication is implemented via transactional outbox in `Rental Service`.
- `Notification Worker` consumes events idempotently using `eventId` deduplication.

API response for rental creation/completion does not wait for notification delivery.

## Consequences
- Better response latency and resilience to notification provider outages.
- Retries and DLQ can be handled without affecting user-facing APIs.
- Additional operational complexity appears in broker/worker monitoring.
