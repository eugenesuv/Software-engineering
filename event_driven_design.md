# Event-Driven Design

Вариант: `21. Система управления арендой автомобилей`.

Lab 06 развивает уже заложенное в проекте решение с `transactional outbox`: доменные операции записывают события в `outbox_events`, а отдельный publisher доставляет их в брокер сообщений. Для практической реализации выбран `RabbitMQ`, потому что для лабораторной важны exchange, routing и очереди потребителей.

## События и команды

| Command | Инициатор | Event | Кому нужно событие |
| --- | --- | --- | --- |
| `RegisterCustomer` | Web/API, Customer | `UserRegistered` | Notification Worker, Reporting/CQRS read model |
| `AddCar` | Web/API, Fleet Manager | `CarAdded` | Search/Catalog read model, Reporting |
| `ChangeCarStatus` | Fleet Service, Rental workflow | `CarStatusChanged` | Catalog read model, Rental availability projection, Reporting |
| `CreateRental` | Web/API, Customer | `RentalCreated` | Notification Worker, Billing/Accounting, CQRS read model, Reporting |
| `CompleteRental` | Web/API, Customer или Fleet Manager | `RentalCompleted` | Notification Worker, Billing/Accounting, CQRS read model, Reporting |

В текущем коде репозитория практически реализованы события аренды: `RentalCreated` и `RentalCompleted` сохраняются в MongoDB/PostgreSQL outbox. Остальные события описаны как часть целевой Event-Driven архитектуры домена.

## Компоненты архитектуры

### Event producers

- `User Service`: публикует `UserRegistered` после успешной регистрации клиента.
- `Fleet Service`: публикует `CarAdded` и `CarStatusChanged` после изменения автопарка.
- `Rental Service`: публикует `RentalCreated` и `RentalCompleted` после изменения жизненного цикла аренды.
- `lab06-producer`: демонстрационный publisher, который читает текущий MongoDB `outbox_events` и отправляет события аренды в RabbitMQ.

### Event consumers

- `Notification Worker`: отправляет email/SMS уведомления клиенту о создании и завершении аренды.
- `CQRS Rental Read Model`: обновляет read model для быстрых запросов по арендам.
- `Reporting/Analytics`: строит отчеты по регистрациям, автопарку и арендам.
- `Billing/Accounting`: получает события аренды для финансового учета.

В Lab 06 `consumer.py` совмещает роль Notification Worker и простой CQRS projection worker: он логирует уведомление и обновляет файл read model.

## Формат событий

Все сообщения публикуются в едином envelope:

```json
{
  "eventId": "evt-001",
  "eventType": "RentalCreated",
  "eventVersion": 1,
  "occurredAt": "2026-04-10T08:40:00Z",
  "producer": "rental-service",
  "aggregate": {
    "type": "rental",
    "id": "20000000-0000-0000-0000-000000000001"
  },
  "payload": {
    "eventId": "evt-001",
    "rentalId": "20000000-0000-0000-0000-000000000001",
    "userId": "00000000-0000-0000-0000-000000000001",
    "carId": "10000000-0000-0000-0000-000000000001",
    "status": "ACTIVE",
    "createdAt": "2026-04-10T08:40:00Z"
  },
  "metadata": {
    "outboxId": "30000000-0000-0000-0000-000000000001",
    "source": "rental_service.outbox_events",
    "delivery": "at-least-once"
  }
}
```

Envelope отделяет технические поля доставки от доменного `payload`. Это упрощает версионирование и позволяет потребителям дедуплицировать сообщения по `eventId`.

## Поток событий

1. Клиент вызывает command endpoint, например `POST /rentals`.
2. Domain service валидирует команду и изменяет write model в своей транзакционной границе.
3. В той же транзакции записывается outbox-событие.
4. `lab06-producer` читает `outbox_events`, преобразует запись в event envelope и публикует сообщение в RabbitMQ.
5. RabbitMQ маршрутизирует сообщение через topic exchange в очереди потребителей.
6. `lab06-consumer` получает сообщение, проверяет `eventId`, обновляет read model и подтверждает обработку manual ack.
7. При повторной доставке consumer пропускает уже обработанный `eventId`, поэтому бизнес-эффект не дублируется.

## RabbitMQ topology

| Объект | Значение |
| --- | --- |
| Exchange | `car_rental.events` |
| Exchange type | `topic` |
| Durable | `true` |
| Routing keys | `rental.created`, `rental.completed`, `user.registered`, `car.added`, `car.status_changed` |
| Demo queue | `notification.rental-events` |
| Demo bindings | `rental.*` |
| DLX | `car_rental.dlx` |
| DLQ | `notification.rental-events.dlq` |

`topic` exchange выбран, потому что потребители могут подписываться на конкретный тип события (`rental.created`) или на группу событий (`rental.*`). Это лучше соответствует доменному каталогу событий, чем один общий queue.

## Гарантии доставки

Используется `at-least-once` delivery:

- producer публикует persistent messages (`delivery_mode=2`);
- producer включает publisher confirms;
- exchange и queue объявлены durable;
- consumer использует manual ack только после сохранения read model;
- poison messages отправляются в DLQ через `car_rental.dlx`;
- consumer обеспечивает идемпотентность по `eventId`.

`exactly-once` на уровне брокера не заявляется. Для доменного эффекта применяется практический подход: сообщение может быть доставлено повторно, но обработчик не выполняет повторное действие для уже обработанного `eventId`.

## CQRS

CQRS применим, потому что операции записи и чтения имеют разные требования:

| Write side | Read side |
| --- | --- |
| `POST /users`, `POST /cars`, `POST /rentals`, `POST /rentals/{id}/complete` | `GET /cars/available`, `GET /rentals/active`, `GET /rentals/history`, отчетные витрины |
| Проверяет инварианты домена и сохраняет агрегаты | Оптимизирован под быстрые запросы и отображение |
| Источник истины: MongoDB/PostgreSQL коллекции и таблицы сервиса | Projection/read model, обновляемая событиями |

В демонстрации Lab 06 write model - это MongoDB `outbox_events`, уже создаваемый текущим приложением и seed-скриптами. Read model хранится consumer'ом в `/state/read_model.json` и синхронизируется событиями `RentalCreated`/`RentalCompleted`.

Синхронизация eventual consistent: сразу после записи команда считается выполненной, а read model догоняет write model после доставки события. Для UI это означает, что отчетные или notification-представления могут обновляться с небольшой задержкой.

## Реализация Lab 06

- `labs/lab06/event_service/producer.py`: читает MongoDB `outbox_events`, публикует события в RabbitMQ.
- `labs/lab06/event_service/consumer.py`: потребляет `rental.*`, выполняет идемпотентную обработку и обновляет CQRS read model.
- `docker-compose.yml`: добавляет `rabbitmq`, `lab06-producer`, `lab06-consumer` под profile `lab06`.
- `event_catalog.md`: фиксирует структуру всех событий, producers, consumers и delivery guarantees.
