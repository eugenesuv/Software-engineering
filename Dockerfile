FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    git \
    libpoco-dev \
    libsqlite3-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN rm -rf build \
 && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j"$(nproc)" \
 && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    libpoco-dev \
    libsqlite3-0 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/build/car_rental_api /usr/local/bin/car_rental_api
COPY openapi.yaml /app/openapi.yaml

VOLUME ["/app/data"]
EXPOSE 8080

HEALTHCHECK --interval=10s --timeout=3s --retries=5 CMD curl -fsS http://127.0.0.1:8080/health || exit 1

ENTRYPOINT ["/usr/local/bin/car_rental_api"]
CMD ["--host", "0.0.0.0", "--port", "8080", "--db-path", "/app/data/car_rental.db", "--jwt-secret", "docker-dev-secret", "--manager-password", "Manager123!"]
