# Домашнее задание 05: Оптимизация производительности через кеширование и rate limiting

Вариант: `21. Система управления арендой автомобилей`.

Lab 05 добавляет к текущему REST API практические механизмы защиты производительности: кеширование горячих чтений каталога автомобилей и ограничение частоты запросов на публичный endpoint авторизации.

## Что добавлено в Lab 05
- in-memory `Cache-Aside` кеш с TTL для горячих endpoint'ов каталога;
- инвалидация кеша после операций, меняющих доступность автомобилей;
- `Fixed Window Counter` rate limiting для `POST /auth/login`;
- HTTP-заголовки `X-Cache`, `X-RateLimit-*` и `Retry-After`;
- документ с анализом hot paths, стратегией кеширования, лимитами и метриками;
- интеграционные тесты для cache hit/miss, инвалидации и `429 Too Many Requests`.

## Артефакты лабораторной
- [`../../performance_design.md`](../../performance_design.md) — анализ производительности, стратегия кеширования и rate limiting.
- [`../../include/car_rental/performance.h`](../../include/car_rental/performance.h), [`../../src/performance.cpp`](../../src/performance.cpp) — TTL cache и fixed-window limiter.
- [`../../tests/performance_tests.cpp`](../../tests/performance_tests.cpp) — регрессионные тесты оптимизаций.
- [`../../openapi.yaml`](../../openapi.yaml) — описание новых response headers и `429`.
- [`../../Dockerfile`](../../Dockerfile), [`../../docker-compose.yml`](../../docker-compose.yml) — запуск приложения.

## Кеширование

Кешируются только публичные read-only endpoints автопарка:

| Endpoint | Ключ кеша | TTL | Инвалидация |
| --- | --- | --- | --- |
| `GET /cars/available` | `cars:available` | 30 секунд | `POST /cars`, `POST /rentals`, `POST /rentals/{rentalId}/complete` |
| `GET /cars/search?class=...` | `cars:class:<CLASS>` | 60 секунд | `POST /cars`, `POST /rentals`, `POST /rentals/{rentalId}/complete` |

Стратегия: `Cache-Aside`.

Кешируемые ответы содержат заголовок:
```text
X-Cache: MISS
X-Cache: HIT
```

Пользовательские данные и аренды не кешируются, потому что они зависят от авторизации и прав доступа.

## Rate limiting

Rate limiting применяется к публичному endpoint'у `POST /auth/login`.

| Endpoint | Алгоритм | Ключ | Лимит |
| --- | --- | --- | --- |
| `POST /auth/login` | `Fixed Window Counter` | IP клиента | 5 запросов / 60 секунд |

Ответы login endpoint содержат:
```text
X-RateLimit-Limit: 5
X-RateLimit-Remaining: 4
X-RateLimit-Reset: 1778160000
```

При превышении лимита API возвращает `429 Too Many Requests` и добавляет:
```text
Retry-After: 42
```

## Локальный запуск

### PostgreSQL backend
```bash
createdb car_rental
psql postgresql://postgres:postgres@127.0.0.1:5432/car_rental -f schema.sql
psql postgresql://postgres:postgres@127.0.0.1:5432/car_rental -f data.sql

cmake -S ../.. -B ../../build
cmake --build ../../build
../../build/car_rental_api \
  --host 127.0.0.1 \
  --port 8080 \
  --data-backend postgres \
  --db-url postgresql://postgres:postgres@127.0.0.1:5432/car_rental \
  --jwt-secret local-dev-secret
```

### MongoDB backend
```bash
../../build/car_rental_api \
  --host 127.0.0.1 \
  --port 8080 \
  --data-backend mongo \
  --mongo-url 'mongodb://127.0.0.1:27017/?replicaSet=rs0' \
  --mongo-db car_rental \
  --jwt-secret local-dev-secret
```

## Docker
```bash
docker compose up --build
```

`docker-compose.yml` поднимает MongoDB replica set `rs0`, применяет `validation.js` и `data.js`, затем запускает API в режиме `DATA_BACKEND=mongo`.

После запуска:
- API: `http://localhost:8080`
- MongoDB: `mongodb://localhost:27017/?replicaSet=rs0`

## Тесты

Основной режим через MongoDB:
```bash
export CAR_RENTAL_TEST_BACKEND=mongo
export CAR_RENTAL_TEST_MONGO_URL='mongodb://127.0.0.1:27017/?replicaSet=rs0'
cmake -S ../.. -B ../../build
cmake --build ../../build
ctest --test-dir ../../build --output-on-failure
```

Регрессия через PostgreSQL:
```bash
export CAR_RENTAL_TEST_BACKEND=postgres
export CAR_RENTAL_TEST_ADMIN_URL='postgresql://postgres:postgres@127.0.0.1:5432/postgres'
cmake -S ../.. -B ../../build
cmake --build ../../build
ctest --test-dir ../../build --output-on-failure
```

Проверяемые сценарии Lab 05:
- первый запрос к `GET /cars/available` возвращает `X-Cache: MISS`, повторный — `X-Cache: HIT`;
- кеш доступных автомобилей инвалидируется после создания аренды;
- кеш поиска по классу инвалидируется после добавления автомобиля;
- шестой запрос к `POST /auth/login` в пределах окна возвращает `429` и rate-limit headers.
