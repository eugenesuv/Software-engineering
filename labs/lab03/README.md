# Домашнее задание 03: Проектирование и оптимизация реляционной БД

Вариант: `21. Система управления арендой автомобилей`.

Lab 03 переводит реализованный в Lab 02 API c `SQLite` на `PostgreSQL` и добавляет полноценные SQL-артефакты: схему, тестовые данные, запросы и документ с оптимизациями.

## Артефакты лабораторной
- [`../../schema.sql`](../../schema.sql) — схема PostgreSQL, ограничения и индексы.
- [`../../data.sql`](../../data.sql) — тестовые данные.
- [`../../queries.sql`](../../queries.sql) — SQL для всех операций варианта.
- [`../../optimization.md`](../../optimization.md) — описание оптимизаций на русском языке и планы `EXPLAIN`.
- [`../../Dockerfile`](../../Dockerfile), [`../../docker-compose.yaml`](../../docker-compose.yaml) — запуск API и PostgreSQL.

## Что изменено в коде
- Публичный HTTP API сохранен без изменений.
- Хранилище переведено на `PostgreSQL`.
- Доступ к данным вынесен в black-box интерфейсы:
  - `UserStore`
  - `FleetStore`
  - `RentalStore`
- Для state-changing сценариев аренды добавлен отдельный `RentalWorkflowCoordinator`, который инкапсулирует транзакционную запись в `fleet_service` и `rental_service`.
- Реализован `PostgreSQL`-адаптер для сервисов вместо прямой работы через `SQLite`.
- Конфиг сервера переведен с `--db-path` на `--db-url` c fallback на `DATABASE_URL`.
- Тестовая инфраструктура теперь создает временную PostgreSQL-базу на каждый сценарий.

## Схема БД

### Доменные примитивы
- `User`
- `Car`
- `Rental`

### Логические границы владения
- `user_service.users`
- `fleet_service.cars`
- `rental_service.rentals`
- `rental_service.outbox_events`

### Ключевые ограничения
- `login` и `vin` уникальны.
- обязательные текстовые поля защищены `NOT NULL` и `CHECK (btrim(...) <> '')`.
- `price_per_day` и `price_total` положительные.
- `start_at < end_at`.
- `closed_at` обязателен только для терминальных статусов аренды.
- активные аренды одного автомобиля не могут пересекаться благодаря `EXCLUDE USING gist`.

## Индексы
- `UNIQUE (login)` — поиск пользователя по логину.
- `GIN lower(first_name/last_name) gin_trgm_ops` — поиск пользователя по маске.
- `UNIQUE (vin)` — защита от дублей VIN.
- `idx_cars_available_created` — список доступных машин.
- `idx_cars_class_created` — поиск по классу.
- `idx_rentals_user_id`, `idx_rentals_car_id` — поддержка FK и точечных выборок.
- `idx_rentals_user_active_created` — активные аренды пользователя.
- `idx_rentals_user_history_created` — история аренды пользователя.
- `idx_outbox_aggregate_id` — индексация FK из outbox в аренды.
- `rentals_no_active_overlap` — гарантия отсутствия пересечений активных аренд.

## Локальный запуск

### Подготовка БД
```bash
createdb car_rental
psql postgresql://postgres:postgres@127.0.0.1:5432/car_rental -f schema.sql
psql postgresql://postgres:postgres@127.0.0.1:5432/car_rental -f data.sql
```

Альтернатива для запуска API: задать `DATABASE_URL` в окружении и не передавать `--db-url`.

### Сборка и запуск API
```bash
cmake -S ../.. -B ../../build
cmake --build ../../build
../../build/car_rental_api \
  --host 127.0.0.1 \
  --port 8080 \
  --db-url postgresql://postgres:postgres@127.0.0.1:5432/car_rental \
  --jwt-secret local-dev-secret
```

Seed-менеджер создается при старте приложения, если пользователя `manager` еще нет:
- login: `manager`
- password: `Manager123!`

## Тесты
Тесты работают против временной PostgreSQL-базы.

```bash
export CAR_RENTAL_TEST_ADMIN_URL=postgresql://postgres:postgres@127.0.0.1:5432/postgres
cmake -S ../.. -B ../../build
cmake --build ../../build
ctest --test-dir ../../build --output-on-failure
```

`CAR_RENTAL_TEST_ADMIN_URL` должен указывать на административную базу, из которой можно создавать и удалять временные БД.

## Docker
```bash
docker compose up --build
```

`docker-compose.yaml` поднимает `postgres` с инициализацией через `schema.sql` и `data.sql`, а затем `api`, подключенный по `DATABASE_URL`.

## Партиционирование
В текущей версии партиционирование только спроектировано:
- при росте истории аренд таблицу `rental_service.rentals` нужно делить на месячные партиции по `start_at`;
- локальные индексы на `user_id + created_at` и overlap-контроль остаются на разделах.
