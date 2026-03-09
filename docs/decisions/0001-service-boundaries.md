# 1. Service boundaries by domain ownership
Date: 2026-03-09

## Status
Accepted

## Context
The system includes three independent domain areas: users, fleet, and rentals.  
Without strict boundaries, services tend to share database access and leak implementation details.

## Decision
Define three domain services with clear ownership:
- `User Service` owns user data and user search APIs.
- `Fleet Service` owns car catalog and availability APIs.
- `Rental Service` owns rental lifecycle APIs and orchestrates rental creation/completion.

Each service writes only to its own schema in PostgreSQL.  
Cross-domain interaction must go through service APIs.

## Consequences
- Modules are replaceable by interface.
- Data contracts stay stable even if storage internals change.
- Some extra network calls are introduced, but coupling is reduced.
