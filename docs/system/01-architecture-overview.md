# Architecture Overview

## Scope
Car Rental Management System (homework variant 21) supports:
- user registration and search;
- fleet management and car availability;
- rental lifecycle (`ACTIVE` -> `COMPLETED`) and rental history.

## Primary Roles
- `Customer` creates and manages rentals.
- `Fleet Manager` adds cars and controls fleet availability.

## External Dependencies
- Driver License Verification Service.
- Payment Provider.
- Email/SMS Provider.
- Message Broker.

## Container Responsibilities
- `Web Application`: UI for customer and fleet manager.
- `API Gateway / BFF`: single entry point and routing to domain services.
- `User Service`: user CRUD/search APIs.
- `Fleet Service`: car catalog and availability APIs.
- `Rental Service`: rental orchestration and status transitions.
- `Notification Worker`: asynchronous notification delivery.
- `Relational Database`: persistent storage, separated by schemas `user`, `fleet`, `rental`.
