# QA Traceability Matrix

Текущая матрица фиксирует покрытие для ДЗ 02 по варианту 21.

| Endpoint | Roles | Happy path | Validation | Auth | Business conflicts | Persistence / side effect |
|---|---|---|---|---|---|---|
| `GET /health` | public | `200` health payload | n/a | n/a | n/a | smoke readiness |
| `POST /users` | public | `201` customer registered | required fields, login/password format, email | n/a | duplicate login `409` | row in `users` |
| `POST /auth/login` | public | `200` JWT issued | missing fields `400` | wrong password `401` | n/a | no mutation |
| `GET /users/by-login` | auth | `200` user returned | missing `login` `400` | no token / bad token / expired token `401` | `404` not found | no mutation |
| `GET /users/search` | auth | `200` filtered collection | empty masks `400` | no token / bad token `401` | n/a | no mutation |
| `POST /cars` | fleet manager | `201` car created | VIN/class/price invalid `400` | no token `401`, wrong role `403` | duplicate VIN `409` | row in `cars` |
| `GET /cars/available` | public | `200` available cars only | n/a | n/a | hides `IN_RENT`/`MAINTENANCE` cars | no mutation |
| `GET /cars/search` | public | `200` by class | invalid class `400` | n/a | n/a | no mutation |
| `POST /rentals` | customer | `201` rental created | missing ids / bad dates `400` | no token `401`, wrong role `403` | unknown user/car `404`, unavailable car `409`, overlapping active rental `409`, invalid license/payment decline `409` | row in `rentals`, car -> `IN_RENT`, outbox event |
| `GET /users/{userId}/rentals/active` | owner or manager | `200` active rentals | malformed path handled as `404` | no token `401`, wrong principal `403` | `404` unknown user | no mutation |
| `GET /users/{userId}/rentals/history` | owner or manager | `200` terminal rentals | malformed path handled as `404` | no token `401`, wrong principal `403` | `404` unknown user | no mutation |
| `POST /rentals/{rentalId}/complete` | owner or manager | `200` rental completed | malformed path handled as `404` | no token `401`, wrong principal `403` | missing rental `404`, already completed `409` | rental -> `COMPLETED`, car -> `AVAILABLE`, outbox event |