CREATE EXTENSION IF NOT EXISTS pg_trgm;
CREATE EXTENSION IF NOT EXISTS btree_gist;

CREATE SCHEMA IF NOT EXISTS user_service;
CREATE SCHEMA IF NOT EXISTS fleet_service;
CREATE SCHEMA IF NOT EXISTS rental_service;

CREATE TABLE IF NOT EXISTS user_service.users (
    id uuid PRIMARY KEY,
    login text NOT NULL UNIQUE,
    password_salt text NOT NULL,
    password_hash text NOT NULL,
    first_name text NOT NULL,
    last_name text NOT NULL,
    email text NOT NULL,
    phone text NOT NULL,
    driver_license_number text NOT NULL,
    role text NOT NULL CHECK (role IN ('CUSTOMER', 'FLEET_MANAGER')),
    created_at timestamptz NOT NULL,
    CONSTRAINT users_login_not_blank CHECK (btrim(login) <> ''),
    CONSTRAINT users_password_salt_not_blank CHECK (btrim(password_salt) <> ''),
    CONSTRAINT users_password_hash_not_blank CHECK (btrim(password_hash) <> ''),
    CONSTRAINT users_first_name_not_blank CHECK (btrim(first_name) <> ''),
    CONSTRAINT users_last_name_not_blank CHECK (btrim(last_name) <> ''),
    CONSTRAINT users_email_not_blank CHECK (btrim(email) <> ''),
    CONSTRAINT users_phone_not_blank CHECK (btrim(phone) <> ''),
    CONSTRAINT users_driver_license_not_blank CHECK (btrim(driver_license_number) <> '')
);

CREATE TABLE IF NOT EXISTS fleet_service.cars (
    id uuid PRIMARY KEY,
    vin text NOT NULL UNIQUE,
    brand text NOT NULL,
    model text NOT NULL,
    class text NOT NULL CHECK (class IN ('ECONOMY', 'COMFORT', 'BUSINESS', 'SUV', 'LUXURY')),
    status text NOT NULL CHECK (status IN ('AVAILABLE', 'RESERVED', 'IN_RENT', 'MAINTENANCE')),
    price_per_day numeric(10, 2) NOT NULL CHECK (price_per_day > 0),
    created_at timestamptz NOT NULL,
    CONSTRAINT cars_vin_not_blank CHECK (btrim(vin) <> ''),
    CONSTRAINT cars_brand_not_blank CHECK (btrim(brand) <> ''),
    CONSTRAINT cars_model_not_blank CHECK (btrim(model) <> '')
);

CREATE TABLE IF NOT EXISTS rental_service.rentals (
    id uuid PRIMARY KEY,
    user_id uuid NOT NULL REFERENCES user_service.users(id),
    car_id uuid NOT NULL REFERENCES fleet_service.cars(id),
    start_at timestamptz NOT NULL,
    end_at timestamptz NOT NULL,
    status text NOT NULL CHECK (status IN ('ACTIVE', 'COMPLETED', 'CANCELLED')),
    price_total numeric(10, 2) NOT NULL CHECK (price_total > 0),
    created_at timestamptz NOT NULL,
    closed_at timestamptz NULL,
    payment_reference text NULL,
    CONSTRAINT rentals_period_valid CHECK (start_at < end_at),
    CONSTRAINT rentals_terminal_closed_at_consistency CHECK (
        (status = 'ACTIVE' AND closed_at IS NULL)
        OR (status IN ('COMPLETED', 'CANCELLED') AND closed_at IS NOT NULL)
    )
);

DO $$
BEGIN
    ALTER TABLE rental_service.rentals
        ADD CONSTRAINT rentals_no_active_overlap
        EXCLUDE USING gist (
            car_id WITH =,
            tstzrange(start_at, end_at, '[)') WITH &&
        )
        WHERE (status = 'ACTIVE');
EXCEPTION
    WHEN duplicate_object THEN NULL;
END
$$;

CREATE TABLE IF NOT EXISTS rental_service.outbox_events (
    id uuid PRIMARY KEY,
    aggregate_type text NOT NULL CHECK (aggregate_type = 'rental'),
    aggregate_id uuid NOT NULL REFERENCES rental_service.rentals(id),
    event_type text NOT NULL CHECK (event_type IN ('RentalCreated', 'RentalCompleted')),
    payload jsonb NOT NULL,
    created_at timestamptz NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_users_first_name_trgm
    ON user_service.users
    USING gin (lower(first_name) gin_trgm_ops);

CREATE INDEX IF NOT EXISTS idx_users_last_name_trgm
    ON user_service.users
    USING gin (lower(last_name) gin_trgm_ops);

CREATE INDEX IF NOT EXISTS idx_cars_available_created
    ON fleet_service.cars (created_at, id)
    WHERE status = 'AVAILABLE';

CREATE INDEX IF NOT EXISTS idx_cars_class_created
    ON fleet_service.cars (class, created_at, id);

CREATE INDEX IF NOT EXISTS idx_rentals_user_id
    ON rental_service.rentals (user_id);

CREATE INDEX IF NOT EXISTS idx_rentals_car_id
    ON rental_service.rentals (car_id);

CREATE INDEX IF NOT EXISTS idx_rentals_user_active_created
    ON rental_service.rentals (user_id, created_at, id)
    WHERE status = 'ACTIVE';

CREATE INDEX IF NOT EXISTS idx_rentals_user_history_created
    ON rental_service.rentals (user_id, created_at, id)
    WHERE status IN ('COMPLETED', 'CANCELLED');

CREATE INDEX IF NOT EXISTS idx_outbox_created_at
    ON rental_service.outbox_events (created_at, id);

CREATE INDEX IF NOT EXISTS idx_outbox_aggregate_id
    ON rental_service.outbox_events (aggregate_id);
