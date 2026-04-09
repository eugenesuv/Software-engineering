-- SQL-запросы для варианта 21.
-- Именованные параметры вида :name приведены для читаемости.

-- 1. Создание нового пользователя
INSERT INTO user_service.users (
    id,
    login,
    password_salt,
    password_hash,
    first_name,
    last_name,
    email,
    phone,
    driver_license_number,
    role,
    created_at
) VALUES (
    :user_id::uuid,
    :login,
    :password_salt,
    :password_hash,
    :first_name,
    :last_name,
    :email,
    :phone,
    :driver_license_number,
    'CUSTOMER',
    :created_at::timestamptz
);

-- 2. Поиск пользователя по логину
SELECT
    id::text AS id,
    login,
    first_name,
    last_name,
    email,
    phone,
    driver_license_number,
    role,
    created_at
FROM user_service.users
WHERE login = :login;

-- 3. Поиск пользователя по маске имени и фамилии
-- Форма запроса согласована с trigram-индексами на lower(first_name) / lower(last_name).
SELECT
    id::text AS id,
    login,
    first_name,
    last_name,
    email,
    phone,
    driver_license_number,
    role,
    created_at
FROM user_service.users
WHERE LOWER(first_name) LIKE '%' || LOWER(:name_mask) || '%'
  AND LOWER(last_name) LIKE '%' || LOWER(:surname_mask) || '%'
ORDER BY created_at ASC, id ASC;

-- 4. Добавление автомобиля в парк
INSERT INTO fleet_service.cars (
    id,
    vin,
    brand,
    model,
    class,
    status,
    price_per_day,
    created_at
) VALUES (
    :car_id::uuid,
    :vin,
    :brand,
    :model,
    :class,
    'AVAILABLE',
    :price_per_day::numeric,
    :created_at::timestamptz
);

-- 5. Получение списка доступных автомобилей
SELECT
    id::text AS id,
    vin,
    brand,
    model,
    class,
    status,
    price_per_day,
    created_at
FROM fleet_service.cars
WHERE status = 'AVAILABLE'
ORDER BY created_at ASC, id ASC;

-- 6. Поиск автомобилей по классу
SELECT
    id::text AS id,
    vin,
    brand,
    model,
    class,
    status,
    price_per_day,
    created_at
FROM fleet_service.cars
WHERE class = :class
ORDER BY created_at ASC, id ASC;

-- 7. Создание аренды
-- До входа в coordinator уже выполнены:
--   - аутентификация и проверка прав;
--   - загрузка пользователя через UserService;
--   - загрузка автомобиля через FleetService;
--   - проверка водительского удостоверения и предавторизация платежа;
--   - расчёт :price_total и получение :payment_reference.
BEGIN;

SELECT
    id::text AS id,
    vin,
    brand,
    model,
    class,
    status,
    price_per_day,
    created_at
FROM fleet_service.cars
WHERE id = :car_id::uuid
FOR UPDATE;

SELECT 1
FROM rental_service.rentals
WHERE car_id = :car_id::uuid
  AND status = 'ACTIVE'
  AND tstzrange(start_at, end_at, '[)')
      && tstzrange(:start_at::timestamptz, :end_at::timestamptz, '[)')
LIMIT 1;

INSERT INTO rental_service.rentals (
    id,
    user_id,
    car_id,
    start_at,
    end_at,
    status,
    price_total,
    created_at,
    closed_at,
    payment_reference
) VALUES (
    :rental_id::uuid,
    :user_id::uuid,
    :car_id::uuid,
    :start_at::timestamptz,
    :end_at::timestamptz,
    'ACTIVE',
    :price_total::numeric,
    :created_at::timestamptz,
    NULL,
    :payment_reference
);

UPDATE fleet_service.cars
SET status = 'IN_RENT'
WHERE id = :car_id::uuid;

INSERT INTO rental_service.outbox_events (
    id,
    aggregate_type,
    aggregate_id,
    event_type,
    payload,
    created_at
) VALUES (
    :create_event_id::uuid,
    'rental',
    :rental_id::uuid,
    'RentalCreated',
    :create_event_payload::jsonb,
    :created_at::timestamptz
);

COMMIT;

-- 8. Получение активных аренд пользователя
SELECT
    id::text AS id,
    user_id::text AS user_id,
    car_id::text AS car_id,
    start_at,
    end_at,
    status,
    price_total,
    created_at,
    closed_at,
    payment_reference
FROM rental_service.rentals
WHERE user_id = :user_id::uuid
  AND status = 'ACTIVE'
ORDER BY created_at ASC, id ASC;

-- 9. Завершение аренды
-- До входа в coordinator уже выполнены:
--   - аутентификация и проверка прав;
--   - чтение аренды для авторизации на read-side;
--   - проверка, что аренда ещё активна.
BEGIN;

SELECT
    id::text AS id,
    user_id::text AS user_id,
    car_id::text AS car_id,
    start_at,
    end_at,
    status,
    price_total,
    created_at,
    closed_at,
    payment_reference
FROM rental_service.rentals
WHERE id = :rental_id::uuid
FOR UPDATE;

UPDATE rental_service.rentals
SET status = 'COMPLETED',
    closed_at = :closed_at::timestamptz
WHERE id = :rental_id::uuid;

UPDATE fleet_service.cars
SET status = 'AVAILABLE'
WHERE id = (
    SELECT car_id
    FROM rental_service.rentals
    WHERE id = :rental_id::uuid
);

INSERT INTO rental_service.outbox_events (
    id,
    aggregate_type,
    aggregate_id,
    event_type,
    payload,
    created_at
) VALUES (
    :complete_event_id::uuid,
    'rental',
    :rental_id::uuid,
    'RentalCompleted',
    :complete_event_payload::jsonb,
    :closed_at::timestamptz
);

COMMIT;

-- 10. Получение истории аренд пользователя
SELECT
    id::text AS id,
    user_id::text AS user_id,
    car_id::text AS car_id,
    start_at,
    end_at,
    status,
    price_total,
    created_at,
    closed_at,
    payment_reference
FROM rental_service.rentals
WHERE user_id = :user_id::uuid
  AND status IN ('COMPLETED', 'CANCELLED')
ORDER BY created_at ASC, id ASC;
