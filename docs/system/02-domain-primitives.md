# Domain Primitives

## Primitive Entities
- `User`: `id`, `login`, `firstName`, `lastName`, contacts.
- `Car`: `id`, `vin`, `brand`, `model`, `class`, `status`.
- `Rental`: `id`, `userId`, `carId`, `startAt`, `endAt`, `status`, payment metadata.

## State Rules
- Car statuses: `AVAILABLE`, `RESERVED`, `IN_RENT`, `MAINTENANCE`.
- Rental statuses: `ACTIVE`, `COMPLETED`, `CANCELLED`.
- A car cannot have overlapping active rentals.

## Black Box Boundary Rules
- Each domain service owns its schema and writes only to its own data.
- Cross-domain reads/writes are done via service APIs, not direct DB coupling.
- Notifications are triggered by domain events, not inline with request processing.
