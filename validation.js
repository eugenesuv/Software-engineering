const collectionNames = ["users", "cars", "rentals", "outbox_events"];

function ensureCollection(name) {
  const exists = db.getCollectionInfos({ name }).length > 0;
  if (!exists) {
    db.createCollection(name);
  }
}

for (const name of collectionNames) {
  ensureCollection(name);
}

db.users.createIndex({ login: 1 }, { name: "uniq_login", unique: true });
db.users.createIndex({ createdAt: 1, _id: 1 }, { name: "idx_users_created" });

db.cars.createIndex({ vin: 1 }, { name: "uniq_vin", unique: true });
db.cars.createIndex({ status: 1, createdAt: 1, _id: 1 }, { name: "idx_cars_status_created" });
db.cars.createIndex({ class: 1, createdAt: 1, _id: 1 }, { name: "idx_cars_class_created" });

db.outbox_events.createIndex({ createdAt: 1, _id: 1 }, { name: "idx_outbox_created" });
db.outbox_events.createIndex({ aggregateId: 1 }, { name: "idx_outbox_aggregate" });

db.runCommand({
  collMod: "rentals",
  validator: {
    $jsonSchema: {
      bsonType: "object",
      required: [
        "_id",
        "userId",
        "carId",
        "userSnapshot",
        "carSnapshot",
        "startAt",
        "endAt",
        "status",
        "priceTotal",
        "createdAt",
        "paymentReference",
        "statusHistory"
      ],
      properties: {
        _id: { bsonType: "string", pattern: "^[0-9a-fA-F-]{36}$" },
        userId: { bsonType: "string", pattern: "^[0-9a-fA-F-]{36}$" },
        carId: { bsonType: "string", pattern: "^[0-9a-fA-F-]{36}$" },
        startAt: { bsonType: "string", pattern: "^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z$" },
        endAt: { bsonType: "string", pattern: "^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z$" },
        status: { enum: ["ACTIVE", "COMPLETED", "CANCELLED"] },
        priceTotal: { bsonType: ["double", "int", "long", "decimal"], minimum: 1 },
        createdAt: { bsonType: "string", pattern: "^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z$" },
        closedAt: { bsonType: "string", pattern: "^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z$" },
        paymentReference: { bsonType: "string", pattern: "^pay-[A-Za-z0-9-]+$" },
        userSnapshot: {
          bsonType: "object",
          required: ["id", "login", "firstName", "lastName", "email", "phone", "driverLicenseNumber", "role", "createdAt"],
          properties: {
            id: { bsonType: "string" },
            login: { bsonType: "string" },
            firstName: { bsonType: "string" },
            lastName: { bsonType: "string" },
            email: { bsonType: "string" },
            phone: { bsonType: "string" },
            driverLicenseNumber: { bsonType: "string" },
            role: { enum: ["CUSTOMER", "FLEET_MANAGER"] },
            createdAt: { bsonType: "string" }
          }
        },
        carSnapshot: {
          bsonType: "object",
          required: ["id", "vin", "brand", "model", "class", "status", "pricePerDay", "createdAt"],
          properties: {
            id: { bsonType: "string" },
            vin: { bsonType: "string" },
            brand: { bsonType: "string" },
            model: { bsonType: "string" },
            class: { enum: ["ECONOMY", "COMFORT", "BUSINESS", "SUV", "LUXURY"] },
            status: { enum: ["AVAILABLE", "RESERVED", "IN_RENT", "MAINTENANCE"] },
            pricePerDay: { bsonType: ["double", "int", "long", "decimal"], minimum: 1 },
            createdAt: { bsonType: "string" }
          }
        },
        statusHistory: {
          bsonType: "array",
          minItems: 1,
          items: {
            bsonType: "object",
            required: ["status", "changedAt", "actor"],
            properties: {
              status: { enum: ["ACTIVE", "COMPLETED", "CANCELLED"] },
              changedAt: { bsonType: "string", pattern: "^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z$" },
              actor: { bsonType: "string" }
            }
          }
        },
        audit: {
          bsonType: "object",
          properties: {
            seededAt: { bsonType: "date" }
          }
        }
      }
    }
  },
  validationLevel: "strict",
  validationAction: "error"
});

db.rentals.createIndex({ userId: 1, status: 1, createdAt: 1, _id: 1 }, { name: "idx_rentals_user_status_created" });
db.rentals.createIndex({ carId: 1, status: 1, startAt: 1, endAt: 1 }, { name: "idx_rentals_car_status_period" });

function expectValidationFailure(document, label) {
  try {
    db.rentals.insertOne(document);
    db.rentals.deleteOne({ _id: document._id });
    throw new Error(`Validation unexpectedly passed: ${label}`);
  } catch (error) {
    if (String(error.message).includes("Validation unexpectedly passed")) {
      throw error;
    }
    print(`Validation OK for ${label}: ${error.message}`);
  }
}

expectValidationFailure(
  {
    _id: "90000000-0000-0000-0000-000000000001",
    carId: "10000000-0000-0000-0000-000000000001",
    status: "ACTIVE",
    priceTotal: 200
  },
  "missing required fields"
);

expectValidationFailure(
  {
    _id: "90000000-0000-0000-0000-000000000002",
    userId: "00000000-0000-0000-0000-000000000001",
    carId: "10000000-0000-0000-0000-000000000001",
    userSnapshot: {
      id: "00000000-0000-0000-0000-000000000001",
      login: "alice.driver",
      firstName: "Alice",
      lastName: "Driver",
      email: "alice.driver@example.com",
      phone: "+79000000001",
      driverLicenseNumber: "DL-ALICE-001",
      role: "CUSTOMER",
      createdAt: "2026-01-10T08:00:00Z"
    },
    carSnapshot: {
      id: "10000000-0000-0000-0000-000000000001",
      vin: "WDB11111111111111",
      brand: "Toyota",
      model: "Corolla",
      class: "ECONOMY",
      status: "AVAILABLE",
      pricePerDay: 55,
      createdAt: "2026-02-01T09:00:00Z"
    },
    startAt: "2026-04-21T09:00:00Z",
    endAt: "2026-04-24T09:00:00Z",
    status: "ACTIVE",
    priceTotal: -1,
    createdAt: "2026-04-20T10:10:00Z",
    paymentReference: "pay-bad-negative",
    statusHistory: [{ status: "ACTIVE", changedAt: "2026-04-20T10:10:00Z", actor: "system" }]
  },
  "negative priceTotal"
);

expectValidationFailure(
  {
    _id: "90000000-0000-0000-0000-000000000003",
    userId: "00000000-0000-0000-0000-000000000001",
    carId: "10000000-0000-0000-0000-000000000001",
    userSnapshot: {
      id: "00000000-0000-0000-0000-000000000001",
      login: "alice.driver",
      firstName: "Alice",
      lastName: "Driver",
      email: "alice.driver@example.com",
      phone: "+79000000001",
      driverLicenseNumber: "DL-ALICE-001",
      role: "CUSTOMER",
      createdAt: "2026-01-10T08:00:00Z"
    },
    carSnapshot: {
      id: "10000000-0000-0000-0000-000000000001",
      vin: "WDB11111111111111",
      brand: "Toyota",
      model: "Corolla",
      class: "ECONOMY",
      status: "AVAILABLE",
      pricePerDay: 55,
      createdAt: "2026-02-01T09:00:00Z"
    },
    startAt: "2026-04-21T09:00:00Z",
    endAt: "2026-04-24T09:00:00Z",
    status: "ACTIVE",
    priceTotal: 200,
    createdAt: "2026-04-20T10:10:00Z",
    paymentReference: "BAD-REF",
    statusHistory: [{ status: "ACTIVE", changedAt: "2026-04-20T10:10:00Z", actor: "system" }]
  },
  "payment reference pattern"
);

print("MongoDB validation is configured successfully.");
