# Проектирование документной модели MongoDB

Вариант: `21. Система управления арендой автомобилей`.

## Коллекции

### `users`
- Назначение: учетные записи клиентов и менеджера автопарка.
- Ключ: строковый UUID в поле `_id`, чтобы сохранить совместимость текущего HTTP API.
- Основные поля:
  - `_id`, `login`, `firstName`, `lastName`, `email`, `phone`, `driverLicenseNumber`, `role`, `createdAt`
  - `credentials.salt`, `credentials.hash`
  - `profile.seededAt` как пример поля типа `Date`
- Индексы:
  - `uniq_login` по `login`
  - `idx_users_created` по `createdAt, _id`

### `cars`
- Назначение: парк автомобилей.
- Основные поля:
  - `_id`, `vin`, `brand`, `model`, `class`, `status`, `pricePerDay`, `createdAt`
  - `features` как массив строк
  - `activeRentalId` для быстрой проверки занятой машины
  - `maintenance.lastServiceAt` как пример вложенного объекта с `Date`
- Индексы:
  - `uniq_vin` по `vin`
  - `idx_cars_status_created`
  - `idx_cars_class_created`

### `rentals`
- Назначение: факты аренды и история их жизненного цикла.
- Основные поля:
  - `_id`, `userId`, `carId`, `startAt`, `endAt`, `status`, `priceTotal`, `createdAt`, `closedAt`, `paymentReference`
  - `userSnapshot` и `carSnapshot`
  - `statusHistory[]`
  - `audit.seededAt` как пример вложенного `Date`
- Индексы:
  - `idx_rentals_user_status_created`
  - `idx_rentals_car_status_period`

### `outbox_events`
- Назначение: outbox для событий аренды.
- Основные поля:
  - `_id`, `aggregateType`, `aggregateId`, `eventType`, `payload`, `createdAt`
  - `meta.emittedAt` как `Date`
- Индексы:
  - `idx_outbox_created`
  - `idx_outbox_aggregate`

## Embedded vs References

### `rentals -> users`
- Хранится `reference` через `userId`.
- Дополнительно хранится `embedded` снимок `userSnapshot`.
- Обоснование:
  - пользователь является самостоятельным агрегатом и читается отдельно;
  - в истории аренды нужен неизменяемый снимок данных на момент бронирования.

### `rentals -> cars`
- Хранится `reference` через `carId`.
- Дополнительно хранится `embedded` снимок `carSnapshot`.
- Обоснование:
  - автомобиль меняет статус и может обновляться отдельно;
  - история аренды должна сохранять марку, модель, класс и цену на момент оформления.

### `rentals -> statusHistory`
- Хранится как `embedded array`.
- Обоснование:
  - история статусов принадлежит только одной аренде;
  - массив ограничен коротким жизненным циклом аренды;
  - обновляется вместе с документом аренды.

### `rentals -> outbox_events`
- Хранится через отдельную коллекцию `outbox_events`.
- Обоснование:
  - outbox нужен как самостоятельный поток событий;
  - удобно считать и читать события глобально;
  - текущая архитектура репозитория уже моделирует outbox как отдельное хранилище.

## Почему именно такая модель
- `users` и `cars` остаются самостоятельными коллекциями, потому что по ним есть отдельные endpoint'ы и независимые сценарии чтения.
- `rentals` денормализована только там, где это уменьшает стоимость чтения истории и не ломает границы агрегатов.
- Строковые ISO-8601 timestamp-поля в core-документах сохраняют полную совместимость с текущим API и существующими DTO.
- Для демонстрации типов MongoDB в seed-данных добавлены массивы, вложенные объекты и поля `Date`, не влияющие на контракт HTTP API.

## Ограничения и инварианты
- `login` и `vin` уникальны на уровне индексов.
- `priceTotal > 0` и `status` ограничен перечислением через `$jsonSchema` для `rentals`.
- Пересечение активных аренд одного автомобиля контролируется в transaction-backed workflow.
- Перевод статусов машины и аренды выполняется в одной MongoDB транзакции.
