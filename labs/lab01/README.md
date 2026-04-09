# Домашнее задание 01: Документирование архитектуры в Structurizr

## 1. Цель и выбранный вариант
- Курс: «Архитектура программных систем».
- Цель: описать архитектуру в подходе Architecture as Code с использованием C4 и Structurizr DSL.
- Вариант: **21. Система управления арендой автомобилей**.
- Референс домена: [Hertz](https://www.hertz.com/).

## 2. Функциональные требования (API)
Система должна поддерживать API:
- `POST /users` — создание нового пользователя.
- `GET /users/by-login?login=...` — поиск пользователя по логину.
- `GET /users/search?nameMask=...&surnameMask=...` — поиск пользователя по маске имени и фамилии.
- `POST /cars` — добавление автомобиля в парк.
- `GET /cars/available` — список доступных автомобилей.
- `GET /cars/search?class=...` — поиск автомобилей по классу.
- `POST /rentals` — создание аренды.
- `GET /users/{userId}/rentals/active` — активные аренды пользователя.
- `POST /rentals/{rentalId}/complete` — завершение аренды.
- `GET /users/{userId}/rentals/history` — история аренд пользователя.

## 3. Роли пользователей
- **Клиент (арендатор)**:
  - регистрируется;
  - ищет автомобили;
  - создает аренду;
  - завершает аренду;
  - просматривает активные аренды и историю.
- **Менеджер автопарка**:
  - добавляет автомобиль в парк;
  - контролирует доступность автомобилей.

## 4. Внешние системы
- **Driver License Verification Service** — проверка водительского удостоверения клиента.
- **Payment Provider** — предавторизация/оплата аренды.
- **Email/SMS Provider** — отправка уведомлений клиенту (подтверждение, завершение аренды).

## 5. Use Case Matrix (роль -> действия)
| Роль | Use case | Основной endpoint |
|---|---|---|
| Клиент | Создать пользователя | `POST /users` |
| Клиент | Найти пользователя по логину | `GET /users/by-login` |
| Клиент | Найти пользователя по маске имени/фамилии | `GET /users/search` |
| Клиент | Посмотреть доступные автомобили | `GET /cars/available` |
| Клиент | Найти автомобили по классу | `GET /cars/search` |
| Клиент | Создать аренду | `POST /rentals` |
| Клиент | Получить активные аренды | `GET /users/{userId}/rentals/active` |
| Клиент | Завершить аренду | `POST /rentals/{rentalId}/complete` |
| Клиент | Получить историю аренд | `GET /users/{userId}/rentals/history` |
| Менеджер автопарка | Добавить автомобиль в парк | `POST /cars` |

Примечание: изменение доступности автомобиля выполняется внутренними операциями сервисов аренды/автопарка и не добавляет отдельный обязательный публичный endpoint сверх ТЗ.

## 6. Контейнерная архитектура (C2)
| Контейнер | Технология | Назначение | Владелец use case |
|---|---|---|---|
| Web Application | React SPA | UI для клиента и менеджера | Вход в систему для всех пользовательских сценариев |
| API Gateway / BFF | Node.js (REST API) | Единая точка входа, авторизация, маршрутизация | Оркестрация вызовов доменных сервисов |
| User Service | REST API | Пользователи и поиск пользователей | `POST /users`, `GET /users/by-login`, `GET /users/search` |
| Fleet Service | REST API | Управление автопарком и доступностью авто | `POST /cars`, `GET /cars/available`, `GET /cars/search` |
| Rental Service | REST API | Создание/завершение аренды, активные и исторические аренды | `POST /rentals`, `GET /.../active`, `POST /rentals/{id}/complete`, `GET /.../history` |
| Notification Worker | Async worker | Отправка уведомлений по событиям аренды | Обработка события `RentalCreated`/`RentalCompleted` |
| Relational Database | PostgreSQL | Хранение доменных данных (логическое разделение по схемам `user`/`fleet`/`rental`) | `users`, `cars`, `rentals` |

## 7. Взаимодействие контейнеров
- `Web Application -> API Gateway / BFF`: `HTTPS/JSON`.
- `API Gateway / BFF -> User/Fleet/Rental Services`: `HTTPS/REST`.
- `User/Fleet/Rental Services -> PostgreSQL`: `SQL`.
- `Rental Service -> Driver License Verification Service`: `HTTPS/REST`.
- `Rental Service -> Payment Provider`: `HTTPS/REST`.
- `Rental Service -> Message Broker`: `Async event publish`.
- `Notification Worker -> Message Broker`: `Async event consume`.
- `Notification Worker -> Email/SMS Provider`: `HTTPS/REST`.

## 8. Основные сценарии взаимодействия
### 8.1 Создание пользователя
1. Клиент отправляет `POST /users` через Web Application.
2. API Gateway / BFF маршрутизирует запрос в User Service.
3. User Service валидирует данные и сохраняет пользователя в PostgreSQL.
4. API Gateway / BFF возвращает результат в Web Application.

### 8.2 Добавление автомобиля в парк
1. Менеджер отправляет `POST /cars` через Web Application.
2. API Gateway / BFF передает запрос в Fleet Service.
3. Fleet Service сохраняет автомобиль в PostgreSQL со статусом `AVAILABLE`.
4. API Gateway / BFF возвращает подтверждение.

### 8.3 Создание аренды
1. Клиент отправляет `POST /rentals`.
2. Rental Service проверяет пользователя, доступность автомобиля, внешние проверки и платеж.
3. Rental Service сохраняет аренду со статусом `ACTIVE`.
4. Fleet Service обновляет статус автомобиля в `IN_RENT`.
5. Rental Service публикует событие `RentalCreated` в Message Broker.
6. API Gateway / BFF возвращает клиенту подтверждение создания аренды.
7. Notification Worker асинхронно отправляет уведомление клиенту.

### 8.4 Завершение аренды
1. Клиент отправляет `POST /rentals/{rentalId}/complete`.
2. Rental Service валидирует текущий статус аренды.
3. Rental Service переводит аренду в `COMPLETED`.
4. Fleet Service возвращает автомобиль в статус `AVAILABLE`.
5. Событие `RentalCompleted` отправляется в Message Broker и далее в Notification Worker.

## 9. Ключевой dynamic-сценарий
Выбран сценарий: **создание аренды**.

Последовательность:
1. Клиент выбирает автомобиль и период аренды в Web Application.
2. Web Application отправляет `POST /rentals` в API Gateway / BFF.
3. API Gateway / BFF передает запрос в Rental Service.
4. Rental Service валидирует пользователя через User Service.
5. Rental Service проверяет доступность авто через Fleet Service.
6. Rental Service запрашивает проверку удостоверения во внешнем сервисе.
7. Rental Service выполняет предавторизацию через Payment Provider.
8. Rental Service сохраняет аренду в PostgreSQL в статусе `ACTIVE`.
9. Rental Service обновляет статус автомобиля через Fleet Service (`IN_RENT`).
10. Rental Service публикует событие `RentalCreated` в Message Broker.
11. API Gateway / BFF возвращает подтверждение в Web Application (без ожидания отправки уведомления).
12. Notification Worker потребляет событие и отправляет уведомление через Email/SMS Provider.

Примечание по надежности: событие `RentalCreated` публикуется из `Rental Service` через `transactional outbox`, а `Notification Worker` обрабатывает сообщения идемпотентно по `eventId` (at-least-once delivery без дубля бизнес-эффекта).

## 10. Принятые технологии и протоколы
- UI: `React SPA`.
- API edge: `Node.js REST API (BFF)`.
- Доменные сервисы: `REST API`.
- Хранилище: `PostgreSQL`.
- Асинхронный транспорт: `Message Broker (Kafka/RabbitMQ)`.
- Синхронные интеграции: `HTTPS/REST`.
- Доступ к БД: `SQL`.
- Асинхронное взаимодействие: `Publish/Subscribe`.

## 11. Модель данных и статусы
### Доменные типы
- `User { id, login, firstName, lastName, phone/email, createdAt }`
- `Car { id, vin, brand, model, class, status }`
- `Rental { id, userId, carId, startAt, endAt, status, price, createdAt, closedAt }`

### Статусы
- `Car.status`: `AVAILABLE`, `RESERVED`, `IN_RENT`, `MAINTENANCE`.
- `Rental.status`: `ACTIVE`, `COMPLETED`, `CANCELLED`.

## 12. Corner Cases и архитектурные правила
- Нельзя создать две активные аренды на один автомобиль в пересекающиеся интервалы.
- `POST /rentals` должен быть идемпотентным по клиентскому ключу запроса.
- `POST /rentals/{rentalId}/complete` для уже завершенной аренды возвращает бизнес-ошибку, без повторного списания/уведомления.
- При сбое после предавторизации, но до фиксации аренды, выполняется компенсация платежа.
- При недоступности Email/SMS Provider событие остается в очереди повторных попыток (retry policy, DLQ).
- Публикация событий аренды из `Rental Service` выполняется через `Transactional Outbox`; `Notification Worker` обрабатывает события идемпотентно (deduplication по `eventId`).
- История аренд включает только терминальные статусы (`COMPLETED`, `CANCELLED`).

## 13. Ограничения и допущения
- Аутентификация инкапсулирована в API Gateway / BFF.
- Отдельный `Auth Service` не выделяется в рамках данного задания.
- Платежи моделируются как внешняя система (предавторизация + финальное списание).
- Поиск по маске имени/фамилии реализуется в User Service (индексы БД).
- При общей `PostgreSQL` логическое владение данными разделено по схемам (`user`, `fleet`, `rental`): сервис пишет только в свою схему, доступ к чужим данным — через API.
- Уведомления отправляются асинхронно через связку `Message Broker + Notification Worker`.
- История аренд строится по `userId` и завершенным статусам.
- Уровень детализации ограничен C1/C2/Dynamic, без C3 (components).

## 14. Критерии готовности и self-check
- Все API из задания сопоставлены контейнерам-владельцам.
- На C1 отображены обе роли пользователей и внешние системы.
- На C2 у каждого контейнера есть технология и ответственность.
- На связях указаны протоколы/типы интеграций.
- Dynamic диаграмма отражает реальный порядок шагов создания аренды.
- Термины и статусы согласованы между `README.md` и `workspace.dsl`.
