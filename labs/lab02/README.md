# Домашнее задание 02: REST API на POCO C++ Libraries

## Контекст
- Курс: «Архитектура программных систем».
- Вариант: `21. Система управления арендой автомобилей`.
- Контекст первой лабораторной сохранён в [`../../workspace.dsl`](../../workspace.dsl) и [`../../docs`](../../docs).

## Что реализовано
- Один REST API на `POCO C++ Libraries`.
- Аутентификация через `JWT`.
- Хранение в `SQLite`.
- Полный набор endpoint'ов варианта 21.
- Seed-пользователь менеджера автопарка.
- OpenAPI спецификация в [`../../openapi.yaml`](../../openapi.yaml).
- Интеграционные HTTP-тесты.
- Dockerfile и `docker-compose.yml`.

## Структура проекта
- [`../../include/car_rental`](../../include/car_rental) — публичные заголовки, DTO, сервисные интерфейсы.
- [`../../src`](../../src) — реализация HTTP-сервера, auth, сервисов и SQLite.
- [`../../tests`](../../tests) — интеграционные тесты.
- [`../../docs/testing/traceability-matrix.md`](../../docs/testing/traceability-matrix.md) — матрица покрытия сценариев.

## API

### Публичные endpoint'ы
- `GET /health`
- `POST /users`
- `POST /auth/login`
- `GET /cars/available`
- `GET /cars/search?class=...`

### Защищённые endpoint'ы
- `GET /users/by-login?login=...`
- `GET /users/search?nameMask=...&surnameMask=...`
- `POST /cars` — только `FLEET_MANAGER`
- `POST /rentals` — только `CUSTOMER`
- `GET /users/{userId}/rentals/active` — владелец или менеджер
- `GET /users/{userId}/rentals/history` — владелец или менеджер
- `POST /rentals/{rentalId}/complete` — владелец или менеджер

### Seed-учётка менеджера
- login: `manager`
- password: `Manager123!`

## Формат данных

### Регистрация пользователя
```json
{
  "login": "alice",
  "password": "Secret123!",
  "firstName": "Alice",
  "lastName": "Driver",
  "email": "alice@example.com",
  "phone": "+79001234567",
  "driverLicenseNumber": "DL-ALICE-001"
}
```

### Логин
```json
{
  "login": "alice",
  "password": "Secret123!"
}
```

### Добавление автомобиля
```json
{
  "vin": "CAR00000000000001",
  "brand": "Toyota",
  "model": "Corolla",
  "class": "ECONOMY",
  "pricePerDay": 150.0
}
```

### Создание аренды
```json
{
  "userId": "user-uuid",
  "carId": "car-uuid",
  "startAt": "2030-04-01T09:00:00Z",
  "endAt": "2030-04-04T09:00:00Z"
}
```

## Сборка и запуск локально

### Зависимости
- `cmake >= 3.20`
- `POCO`
- `SQLite3`
- компилятор с поддержкой `C++17`

### macOS через Homebrew
```bash
brew install cmake poco doctest
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/car_rental_api --host 127.0.0.1 --port 8080 --db-path data/car_rental.db --jwt-secret local-dev-secret
```

### Linux
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/car_rental_api --host 0.0.0.0 --port 8080 --db-path data/car_rental.db --jwt-secret local-dev-secret
```

## Docker
```bash
docker compose up --build
```

Сервис будет доступен на `http://localhost:8080`.

## Примеры `curl`

### 1. Регистрация
```bash
curl -X POST http://localhost:8080/users \
  -H 'Content-Type: application/json' \
  -d '{
    "login":"alice",
    "password":"Secret123!",
    "firstName":"Alice",
    "lastName":"Driver",
    "email":"alice@example.com",
    "phone":"+79001234567",
    "driverLicenseNumber":"DL-ALICE-001"
  }'
```

### 2. Логин
```bash
curl -X POST http://localhost:8080/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"login":"alice","password":"Secret123!"}'
```

### 3. Добавление автомобиля менеджером
```bash
MANAGER_TOKEN=$(curl -s -X POST http://localhost:8080/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"login":"manager","password":"Manager123!"}' | jq -r '.accessToken')

curl -X POST http://localhost:8080/cars \
  -H "Authorization: Bearer ${MANAGER_TOKEN}" \
  -H 'Content-Type: application/json' \
  -d '{
    "vin":"CAR00000000000001",
    "brand":"Toyota",
    "model":"Corolla",
    "class":"ECONOMY",
    "pricePerDay":150.0
  }'
```

### 4. Создание аренды клиентом
```bash
CUSTOMER_TOKEN=...
USER_ID=...
CAR_ID=...

curl -X POST http://localhost:8080/rentals \
  -H "Authorization: Bearer ${CUSTOMER_TOKEN}" \
  -H 'Content-Type: application/json' \
  -d "{
    \"userId\":\"${USER_ID}\",
    \"carId\":\"${CAR_ID}\",
    \"startAt\":\"2030-04-01T09:00:00Z\",
    \"endAt\":\"2030-04-04T09:00:00Z\"
  }"
```

## Тестирование
- Тесты запускают `POCO HTTPServer` на случайном порту.
- Проверяются happy-path, validation, auth, бизнес-конфликты и побочные эффекты в `SQLite`.
- Отдельно проверяется наличие и базовая полнота `openapi.yaml`.

Запуск:
```bash
ctest --test-dir build --output-on-failure
```
