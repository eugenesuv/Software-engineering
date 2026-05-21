# Домашнее задание 06: Проектирование Event-Driven архитектуры

Вариант: `21. Система управления арендой автомобилей`.

Lab 06 добавляет к текущему проекту Event-Driven слой на базе `RabbitMQ`: демонстрационный producer читает существующий MongoDB `outbox_events`, публикует события аренды в broker, а consumer обрабатывает их идемпотентно и обновляет простую CQRS read model.

## Что добавлено в Lab 06

- анализ commands/events для домена аренды автомобилей;
- Event-Driven архитектура с producers, consumers и потоком событий;
- `RabbitMQ` topic exchange, routing keys, durable queue и DLQ;
- CQRS-разделение write/read модели для аренд;
- Python producer/consumer для проверки доставки событий;
- event catalog с payload, producer, consumers и delivery guarantees.

## Артефакты лабораторной

- [`../../event_driven_design.md`](../../event_driven_design.md) - описание Event-Driven архитектуры.
- [`../../event_catalog.md`](../../event_catalog.md) - каталог событий системы.
- [`event_service/producer.py`](event_service/producer.py) - publisher из MongoDB outbox в RabbitMQ.
- [`event_service/consumer.py`](event_service/consumer.py) - consumer с идемпотентной обработкой и CQRS read model.
- [`event_service/Dockerfile`](event_service/Dockerfile), [`event_service/requirements.txt`](event_service/requirements.txt) - контейнеризация Python worker'ов.
- [`../../docker-compose.yml`](../../docker-compose.yml) - запуск RabbitMQ и worker'ов через profile `lab06`.

## RabbitMQ topology

| Объект | Значение |
| --- | --- |
| Exchange | `car_rental.events` |
| Type | `topic` |
| Queue | `notification.rental-events` |
| Bindings | `rental.*` |
| DLX | `car_rental.dlx` |
| DLQ | `notification.rental-events.dlq` |

Routing keys:

| Event | Routing key |
| --- | --- |
| `RentalCreated` | `rental.created` |
| `RentalCompleted` | `rental.completed` |
| `UserRegistered` | `user.registered` |
| `CarAdded` | `car.added` |
| `CarStatusChanged` | `car.status_changed` |

## Docker запуск

В первом терминале запустите MongoDB seed, RabbitMQ и consumer:

```bash
docker compose --profile lab06 up --build rabbitmq mongo mongo-init lab06-consumer
```

Во втором терминале опубликуйте события из MongoDB outbox:

```bash
docker compose --profile lab06 run --rm lab06-producer
```

RabbitMQ Management UI:

```text
http://localhost:15672
login: guest
password: guest
```

Consumer хранит read model в Docker volume по пути контейнера:

```text
/state/read_model.json
```

Посмотреть read model можно так:

```bash
docker compose --profile lab06 exec lab06-consumer python -m json.tool /state/read_model.json
```

## Проверка идемпотентности

Повторно запустите producer:

```bash
docker compose --profile lab06 run --rm lab06-producer
```

Consumer получит те же сообщения, но события с уже обработанным `eventId` будут пропущены:

```text
duplicate ignored eventId=evt-001
```

Это демонстрирует практическую exactly-once обработку бизнес-эффекта поверх `at-least-once` доставки брокера.

## Локальный запуск без Docker worker'ов

После запуска MongoDB и RabbitMQ:

```bash
python -m venv .venv
. .venv/bin/activate
pip install -r labs/lab06/event_service/requirements.txt

export MONGO_URL='mongodb://127.0.0.1:27017/?replicaSet=rs0'
export MONGO_DB_NAME=car_rental
export RABBITMQ_URL='amqp://guest:guest@127.0.0.1:5672/%2F'
export CQRS_STATE_PATH=/tmp/car-rental-read-model.json

python labs/lab06/event_service/consumer.py
python labs/lab06/event_service/producer.py
```

## Тесты

Статическая проверка Python-кода:

```bash
python -m py_compile labs/lab06/event_service/*.py
```

Проверка основной C++ сборки:

```bash
cmake -S ../.. -B ../../build
cmake --build ../../build
```

Проверяемые сценарии Lab 06:

- producer публикует `RentalCreated` и `RentalCompleted` из `outbox_events`;
- RabbitMQ маршрутизирует сообщения по `rental.created` и `rental.completed`;
- consumer подтверждает сообщения manual ack после обновления read model;
- повторная доставка не дублирует бизнес-эффект из-за дедупликации по `eventId`;
- некорректные сообщения попадают в DLQ.
