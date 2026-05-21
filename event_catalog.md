# Event Catalog

Вариант: `21. Система управления арендой автомобилей`.

Все события публикуются через durable topic exchange `car_rental.events` в формате JSON envelope. Гарантия доставки для событий каталога - `at-least-once`; потребители должны быть идемпотентны по `eventId`.

## Общий envelope

| Поле | Тип | Описание |
| --- | --- | --- |
| `eventId` | string | Уникальный идентификатор события для дедупликации |
| `eventType` | string | Название события |
| `eventVersion` | number | Версия схемы события |
| `occurredAt` | string | Время возникновения события в ISO 8601 UTC |
| `producer` | string | Сервис-производитель |
| `aggregate.type` | string | Тип агрегата: `user`, `car`, `rental` |
| `aggregate.id` | string | Идентификатор агрегата |
| `payload` | object | Доменная полезная нагрузка |
| `metadata` | object | Технические поля доставки и трассировки |

## UserRegistered

| Свойство | Значение |
| --- | --- |
| Command | `RegisterCustomer` |
| Producer | `User Service` |
| Routing key | `user.registered` |
| Consumers | `Notification Worker`, `Reporting/CQRS read model` |
| Delivery | `at-least-once`, идемпотентность по `eventId` |

Payload:

```json
{
  "eventId": "evt-user-001",
  "userId": "00000000-0000-0000-0000-000000000001",
  "login": "ivan.petrov",
  "email": "ivan.petrov@example.com",
  "phone": "+79990000001",
  "role": "CUSTOMER",
  "registeredAt": "2026-04-10T08:30:00Z"
}
```

Назначение: уведомить клиента о регистрации и обновить пользовательскую read model для отчетов.

## CarAdded

| Свойство | Значение |
| --- | --- |
| Command | `AddCar` |
| Producer | `Fleet Service` |
| Routing key | `car.added` |
| Consumers | `Search/Catalog read model`, `Reporting` |
| Delivery | `at-least-once`, идемпотентность по `eventId` |

Payload:

```json
{
  "eventId": "evt-car-001",
  "carId": "10000000-0000-0000-0000-000000000001",
  "vin": "XTA21099000000001",
  "brand": "Toyota",
  "model": "Camry",
  "class": "COMFORT",
  "status": "AVAILABLE",
  "pricePerDay": 68.0,
  "createdAt": "2026-04-10T08:35:00Z"
}
```

Назначение: добавить автомобиль в поисковые и отчетные проекции.

## CarStatusChanged

| Свойство | Значение |
| --- | --- |
| Command | `ChangeCarStatus`, rental workflow status transition |
| Producer | `Fleet Service` |
| Routing key | `car.status_changed` |
| Consumers | `Search/Catalog read model`, `Rental availability projection`, `Reporting` |
| Delivery | `at-least-once`, идемпотентность по `eventId` |

Payload:

```json
{
  "eventId": "evt-car-status-001",
  "carId": "10000000-0000-0000-0000-000000000001",
  "previousStatus": "AVAILABLE",
  "newStatus": "IN_RENT",
  "reason": "RentalCreated",
  "changedAt": "2026-04-10T08:40:00Z"
}
```

Назначение: синхронизировать доступность автомобиля в read models после ручного изменения статуса или изменения аренды.

## RentalCreated

| Свойство | Значение |
| --- | --- |
| Command | `CreateRental` |
| Producer | `Rental Service`; в demo - `lab06-producer` из MongoDB outbox |
| Routing key | `rental.created` |
| Consumers | `Notification Worker`, `Billing/Accounting`, `CQRS Rental Read Model`, `Reporting` |
| Delivery | `at-least-once`, persistent message, publisher confirm, manual ack, DLQ |

Payload:

```json
{
  "eventId": "evt-001",
  "rentalId": "20000000-0000-0000-0000-000000000001",
  "userId": "00000000-0000-0000-0000-000000000001",
  "carId": "10000000-0000-0000-0000-000000000001",
  "status": "ACTIVE",
  "createdAt": "2026-04-10T08:40:00Z"
}
```

Назначение: отправить клиенту уведомление о создании аренды, обновить rental read model и передать событие в финансовые/отчетные контуры.

## RentalCompleted

| Свойство | Значение |
| --- | --- |
| Command | `CompleteRental` |
| Producer | `Rental Service`; в demo - `lab06-producer` из MongoDB outbox |
| Routing key | `rental.completed` |
| Consumers | `Notification Worker`, `Billing/Accounting`, `CQRS Rental Read Model`, `Reporting` |
| Delivery | `at-least-once`, persistent message, publisher confirm, manual ack, DLQ |

Payload:

```json
{
  "eventId": "evt-011",
  "rentalId": "20000000-0000-0000-0000-000000000005",
  "userId": "00000000-0000-0000-0000-000000000005",
  "carId": "10000000-0000-0000-0000-000000000005",
  "status": "COMPLETED",
  "createdAt": "2026-03-05T09:05:00Z"
}
```

Назначение: уведомить клиента о завершении аренды, обновить историю аренды в read model и передать финальное состояние в финансовый учет.

## Demo coverage

Lab 06 фактически публикует и обрабатывает `RentalCreated` и `RentalCompleted`, потому что эти события уже есть в текущем `outbox_events`. `UserRegistered`, `CarAdded` и `CarStatusChanged` описаны в каталоге как целевые события системы и используют тот же envelope, exchange и delivery policy.
