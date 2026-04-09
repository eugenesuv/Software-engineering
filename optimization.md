# Оптимизация запросов PostgreSQL

## Предположения о нагрузке
- `GET /users/search` часто используется для операторского поиска по части имени и фамилии.
- `GET /cars/available` является одним из самых частых сценариев чтения.
- `GET /cars/search?class=...` регулярно фильтрует каталог по классу автомобиля.
- `GET /users/{userId}/rentals/active` и `GET /users/{userId}/rentals/history` являются основными пользовательскими выборками по арендам.
- `POST /rentals` чувствителен к конкуренции и должен быстро определять пересечение активных аренд одного автомобиля.

## Обоснование индексов
- `user_service.users(login)` покрывается `UNIQUE` и ускоряет точный поиск пользователя по логину, а также сценарий логина.
- `idx_users_first_name_trgm` и `idx_users_last_name_trgm` ускоряют поиск по маске через `LOWER(first_name) LIKE ...` и `LOWER(last_name) LIKE ...`.
- `fleet_service.cars(vin)` покрывается `UNIQUE` и защищает парк от дублей VIN.
- `idx_cars_available_created` соответствует запросу `WHERE status = 'AVAILABLE' ORDER BY created_at, id`.
- `idx_cars_class_created` соответствует запросу `WHERE class = ? ORDER BY created_at, id`.
- `idx_rentals_user_id` и `idx_rentals_car_id` нужны для поддержки FK, соединений и точечных выборок по владельцу и автомобилю.
- `idx_rentals_user_active_created` оптимизирует чтение активных аренд пользователя.
- `idx_rentals_user_history_created` готовит схему к большому объёму завершённых аренд и к дальнейшему партиционированию.
- `idx_outbox_created_at` ускоряет последовательное чтение outbox-таблицы фоновым обработчиком.
- `idx_outbox_aggregate_id` закрывает индексирование FK `rental_service.outbox_events.aggregate_id`.
- `rentals_no_active_overlap` одновременно реализует бизнес-инвариант и даёт PostgreSQL эффективный путь доступа для проверки пересечения временных интервалов.

## Планы выполнения до и после оптимизации

### 1. Поиск пользователя по маске
До оптимизации:
```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, login
FROM user_service.users
WHERE lower(first_name) LIKE '%anna%'
  AND lower(last_name) LIKE '%ivanov%'
ORDER BY created_at ASC, id ASC;
```

План до добавления trigram-индексов:
```text
Sort  (cost=835.21..835.22 rows=1 width=39) (actual time=3.419..3.436 rows=666.00 loops=1)
  Sort Key: created_at, id
  Sort Method: quicksort  Memory: 61kB
  Buffers: shared hit=441
  ->  Seq Scan on users  (cost=0.00..835.20 rows=1 width=39) (actual time=0.016..3.355 rows=666.00 loops=1)
        Filter: ((lower(first_name) ~~ '%anna%'::text) AND (lower(last_name) ~~ '%ivanov%'::text))
        Rows Removed by Filter: 19344
        Buffers: shared hit=435
Planning:
  Buffers: shared hit=114
Planning Time: 0.242 ms
Execution Time: 3.487 ms
```

План после добавления trigram-индексов:
```text
Sort  (cost=60.90..60.91 rows=1 width=39) (actual time=0.836..0.849 rows=666.00 loops=1)
  Sort Key: created_at, id
  Sort Method: quicksort  Memory: 61kB
  Buffers: shared hit=444
  ->  Bitmap Heap Scan on users  (cost=38.89..60.89 rows=1 width=39) (actual time=0.112..0.797 rows=666.00 loops=1)
        Recheck Cond: (lower(last_name) ~~ '%ivanov%'::text)
        Filter: (lower(first_name) ~~ '%anna%'::text)
        Rows Removed by Filter: 668
        Heap Blocks: exact=435
        Buffers: shared hit=444
        ->  Bitmap Index Scan on idx_users_last_name_trgm  (cost=0.00..38.89 rows=6 width=0) (actual time=0.087..0.087 rows=1334.00 loops=1)
              Index Cond: (lower(last_name) ~~ '%ivanov%'::text)
              Index Searches: 1
              Buffers: shared hit=9
Planning:
  Buffers: shared hit=20
Planning Time: 0.133 ms
Execution Time: 0.890 ms
```

### 2. Список доступных автомобилей
До оптимизации:
```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, vin, brand, model, class, status, price_per_day, created_at
FROM fleet_service.cars
WHERE status = 'AVAILABLE'
ORDER BY created_at ASC, id ASC;
```

План до частичного индекса:
```text
Sort  (cost=582.66..595.17 rows=5005 width=86) (actual time=1.051..1.143 rows=5005.00 loops=1)
  Sort Key: created_at, id
  Sort Method: quicksort  Memory: 723kB
  Buffers: shared hit=156
  ->  Seq Scan on cars  (cost=0.00..275.12 rows=5005 width=86) (actual time=0.005..0.785 rows=5005.00 loops=1)
        Filter: (status = 'AVAILABLE'::text)
        Rows Removed by Filter: 5005
        Buffers: shared hit=150
Planning:
  Buffers: shared hit=142
Planning Time: 0.251 ms
Execution Time: 1.260 ms
```

План после частичного индекса:
```text
Index Scan using idx_cars_available_created on cars  (cost=0.28..489.73 rows=5005 width=86) (actual time=0.008..0.293 rows=5005.00 loops=1)
  Index Searches: 1
  Buffers: shared hit=150 read=26
Planning:
  Buffers: shared hit=5 read=1
Planning Time: 0.083 ms
Execution Time: 0.398 ms
```

### 3. Активные аренды пользователя
До оптимизации:
```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, user_id, car_id, status, created_at
FROM rental_service.rentals
WHERE user_id = '00000000-0000-0000-0000-000000000001'::uuid
  AND status = 'ACTIVE'
ORDER BY created_at ASC, id ASC;
```

План до частичного индекса по активным арендам:
```text
Sort  (cost=11.31..11.32 rows=1 width=65) (actual time=0.020..0.020 rows=1.00 loops=1)
  Sort Key: created_at, id
  Sort Method: quicksort  Memory: 25kB
  Buffers: shared hit=9
  ->  Index Scan using idx_rentals_user_id on rentals  (cost=0.29..11.30 rows=1 width=65) (actual time=0.008..0.008 rows=1.00 loops=1)
        Index Cond: (user_id = '00000000-0000-0000-0000-000000000001'::uuid)
        Filter: (status = 'ACTIVE'::text)
        Index Searches: 1
        Buffers: shared hit=3
Planning:
  Buffers: shared hit=187
Planning Time: 0.490 ms
Execution Time: 0.033 ms
```

План после частичного индекса по активным арендам:
```text
Index Scan using idx_rentals_user_active_created on rentals  (cost=0.13..8.15 rows=1 width=65) (actual time=0.006..0.007 rows=1.00 loops=1)
  Index Cond: (user_id = '00000000-0000-0000-0000-000000000001'::uuid)
  Index Searches: 1
  Buffers: shared hit=1 read=1
Planning:
  Buffers: shared hit=5 read=1
Planning Time: 0.096 ms
Execution Time: 0.009 ms
```

### 4. История аренд пользователя
До оптимизации:
```sql
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, user_id, car_id, status, created_at, closed_at
FROM rental_service.rentals
WHERE user_id = '90000000-0000-0000-0000-000000000001'::uuid
  AND status IN ('COMPLETED', 'CANCELLED')
ORDER BY created_at ASC, id ASC;
```

План до пользовательских индексов по арендам:
```text
Sort  (cost=1762.72..1775.20 rows=4991 width=73) (actual time=4.129..4.237 rows=5002.00 loops=1)
  Sort Key: created_at, id
  Sort Method: quicksort  Memory: 661kB
  Buffers: shared hit=787
  ->  Seq Scan on rentals  (cost=0.00..1456.15 rows=4991 width=73) (actual time=0.006..3.511 rows=5002.00 loops=1)
        Filter: ((status = ANY ('{COMPLETED,CANCELLED}'::text[])) AND (user_id = '90000000-0000-0000-0000-000000000001'::uuid))
        Rows Removed by Filter: 40008
        Buffers: shared hit=781
Planning:
  Buffers: shared hit=230
Planning Time: 0.422 ms
Execution Time: 4.369 ms
```

План после добавления пользовательских индексов:
```text
Sort  (cost=1261.43..1273.91 rows=4991 width=73) (actual time=1.016..1.109 rows=5002.00 loops=1)
  Sort Key: created_at, id
  Sort Method: quicksort  Memory: 661kB
  Buffers: shared hit=94 read=6
  ->  Bitmap Heap Scan on rentals  (cost=98.98..954.86 rows=4991 width=73) (actual time=0.064..0.534 rows=5002.00 loops=1)
        Recheck Cond: (user_id = '90000000-0000-0000-0000-000000000001'::uuid)
        Filter: (status = ANY ('{COMPLETED,CANCELLED}'::text[]))
        Heap Blocks: exact=94
        Buffers: shared hit=94 read=6
        ->  Bitmap Index Scan on idx_rentals_user_id  (cost=0.00..97.73 rows=4992 width=0) (actual time=0.054..0.054 rows=5002.00 loops=1)
              Index Cond: (user_id = '90000000-0000-0000-0000-000000000001'::uuid)
              Index Searches: 1
              Buffers: shared read=6
Planning:
  Buffers: shared hit=5 read=2
Planning Time: 0.199 ms
Execution Time: 1.217 ms
```

Примечание: на текущем синтетическом распределении планировщик всё ещё выбирает `idx_rentals_user_id`, а не `idx_rentals_user_history_created`. Это не ошибка схемы: частичный индекс по истории станет полезнее при перекосе данных в сторону терминальных статусов или после партиционирования истории по времени.

## Почему итоговые планы лучше
- Поиск пользователя по маске уходит от полного последовательного сканирования к плану с опорой на индекс и на синтетическом наборе данных сокращается с `3.487 ms` до `0.890 ms`.
- Список доступных автомобилей перестаёт сканировать и сортировать весь каталог, превращаясь в упорядоченный `Index Scan`; время снижается с `1.260 ms` до `0.398 ms`.
- Чтение активных аренд пользователя переходит от общего индекса по `user_id` с постфильтрацией к более узкому частичному индексу, снижая время с `0.033 ms` до `0.009 ms`.
- История аренд пользователя уходит от полного `Seq Scan` (`4.369 ms`) к плану с опорой на индекс (`1.217 ms`) после добавления пользовательских индексов.
- `rentals_no_active_overlap` переносит контроль пересечения аренд из прикладной логики в PostgreSQL и защищает инвариант даже при конкурентных записях.
- Индекс `idx_outbox_aggregate_id` закрывает обязательную индексацию FK для технической таблицы outbox и снижает стоимость проверки ссылочной целостности.

## Замечание по партиционированию
- На текущем объёме данных физическое партиционирование не требуется.
- При росте истории аренд до десятков миллионов строк таблицу `rental_service.rentals` нужно делить на месячные партиции по `start_at`.
- На каждой партиции должны остаться локальные индексы для активных и исторических выборок пользователя, а также механизм защиты от пересечения активных аренд.
