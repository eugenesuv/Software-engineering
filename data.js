const users = [
  {
    _id: "00000000-0000-0000-0000-000000000001",
    login: "alice.driver",
    firstName: "Alice",
    lastName: "Driver",
    email: "alice.driver@example.com",
    phone: "+79000000001",
    driverLicenseNumber: "DL-ALICE-001",
    role: "CUSTOMER",
    createdAt: "2026-01-10T08:00:00Z",
    credentials: { salt: "seed-salt-01", hash: "seed-hash-01" },
    profile: { seededAt: new Date("2026-01-10T08:00:00Z"), loyaltyLevel: 1 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000002",
    login: "alex.wong",
    firstName: "Alex",
    lastName: "Wong",
    email: "alex.wong@example.com",
    phone: "+79000000002",
    driverLicenseNumber: "DL-ALEX-002",
    role: "CUSTOMER",
    createdAt: "2026-01-11T08:00:00Z",
    credentials: { salt: "seed-salt-02", hash: "seed-hash-02" },
    profile: { seededAt: new Date("2026-01-11T08:00:00Z"), loyaltyLevel: 2 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000003",
    login: "anna.lee",
    firstName: "Anna",
    lastName: "Lee",
    email: "anna.lee@example.com",
    phone: "+79000000003",
    driverLicenseNumber: "DL-ANNA-003",
    role: "CUSTOMER",
    createdAt: "2026-01-12T08:00:00Z",
    credentials: { salt: "seed-salt-03", hash: "seed-hash-03" },
    profile: { seededAt: new Date("2026-01-12T08:00:00Z"), loyaltyLevel: 1 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000004",
    login: "boris.kim",
    firstName: "Boris",
    lastName: "Kim",
    email: "boris.kim@example.com",
    phone: "+79000000004",
    driverLicenseNumber: "DL-BORIS-004",
    role: "CUSTOMER",
    createdAt: "2026-01-13T08:00:00Z",
    credentials: { salt: "seed-salt-04", hash: "seed-hash-04" },
    profile: { seededAt: new Date("2026-01-13T08:00:00Z"), loyaltyLevel: 3 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000005",
    login: "carla.ivanova",
    firstName: "Carla",
    lastName: "Ivanova",
    email: "carla.ivanova@example.com",
    phone: "+79000000005",
    driverLicenseNumber: "DL-CARLA-005",
    role: "CUSTOMER",
    createdAt: "2026-01-14T08:00:00Z",
    credentials: { salt: "seed-salt-05", hash: "seed-hash-05" },
    profile: { seededAt: new Date("2026-01-14T08:00:00Z"), loyaltyLevel: 2 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000006",
    login: "dmitry.sokolov",
    firstName: "Dmitry",
    lastName: "Sokolov",
    email: "dmitry.sokolov@example.com",
    phone: "+79000000006",
    driverLicenseNumber: "DL-DMITRY-006",
    role: "CUSTOMER",
    createdAt: "2026-01-15T08:00:00Z",
    credentials: { salt: "seed-salt-06", hash: "seed-hash-06" },
    profile: { seededAt: new Date("2026-01-15T08:00:00Z"), loyaltyLevel: 1 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000007",
    login: "elena.novik",
    firstName: "Elena",
    lastName: "Novik",
    email: "elena.novik@example.com",
    phone: "+79000000007",
    driverLicenseNumber: "DL-ELENA-007",
    role: "CUSTOMER",
    createdAt: "2026-01-16T08:00:00Z",
    credentials: { salt: "seed-salt-07", hash: "seed-hash-07" },
    profile: { seededAt: new Date("2026-01-16T08:00:00Z"), loyaltyLevel: 1 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000008",
    login: "farid.amin",
    firstName: "Farid",
    lastName: "Amin",
    email: "farid.amin@example.com",
    phone: "+79000000008",
    driverLicenseNumber: "DL-FARID-008",
    role: "CUSTOMER",
    createdAt: "2026-01-17T08:00:00Z",
    credentials: { salt: "seed-salt-08", hash: "seed-hash-08" },
    profile: { seededAt: new Date("2026-01-17T08:00:00Z"), loyaltyLevel: 2 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000009",
    login: "grace.cho",
    firstName: "Grace",
    lastName: "Cho",
    email: "grace.cho@example.com",
    phone: "+79000000009",
    driverLicenseNumber: "DL-GRACE-009",
    role: "CUSTOMER",
    createdAt: "2026-01-18T08:00:00Z",
    credentials: { salt: "seed-salt-09", hash: "seed-hash-09" },
    profile: { seededAt: new Date("2026-01-18T08:00:00Z"), loyaltyLevel: 3 }
  },
  {
    _id: "00000000-0000-0000-0000-000000000010",
    login: "ivan.petrov",
    firstName: "Ivan",
    lastName: "Petrov",
    email: "ivan.petrov@example.com",
    phone: "+79000000010",
    driverLicenseNumber: "DL-IVAN-010",
    role: "CUSTOMER",
    createdAt: "2026-01-19T08:00:00Z",
    credentials: { salt: "seed-salt-10", hash: "seed-hash-10" },
    profile: { seededAt: new Date("2026-01-19T08:00:00Z"), loyaltyLevel: 2 }
  }
];

const cars = [
  {
    _id: "10000000-0000-0000-0000-000000000001",
    vin: "WDB11111111111111",
    brand: "Toyota",
    model: "Corolla",
    class: "ECONOMY",
    status: "IN_RENT",
    pricePerDay: 55,
    createdAt: "2026-02-01T09:00:00Z",
    activeRentalId: "20000000-0000-0000-0000-000000000001",
    features: ["air_conditioning", "bluetooth", "usb"],
    maintenance: { lastServiceAt: new Date("2026-02-01T07:00:00Z"), mileageKm: 18420 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000002",
    vin: "WDB22222222222222",
    brand: "Hyundai",
    model: "Elantra",
    class: "COMFORT",
    status: "IN_RENT",
    pricePerDay: 72,
    createdAt: "2026-02-02T09:00:00Z",
    activeRentalId: "20000000-0000-0000-0000-000000000002",
    features: ["air_conditioning", "carplay"],
    maintenance: { lastServiceAt: new Date("2026-02-02T07:00:00Z"), mileageKm: 21100 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000003",
    vin: "WDB33333333333333",
    brand: "BMW",
    model: "320",
    class: "BUSINESS",
    status: "IN_RENT",
    pricePerDay: 120,
    createdAt: "2026-02-03T09:00:00Z",
    activeRentalId: "20000000-0000-0000-0000-000000000003",
    features: ["air_conditioning", "leather", "navigation"],
    maintenance: { lastServiceAt: new Date("2026-02-03T07:00:00Z"), mileageKm: 32500 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000004",
    vin: "WDB44444444444444",
    brand: "Toyota",
    model: "RAV4",
    class: "SUV",
    status: "IN_RENT",
    pricePerDay: 95,
    createdAt: "2026-02-04T09:00:00Z",
    activeRentalId: "20000000-0000-0000-0000-000000000004",
    features: ["air_conditioning", "awd"],
    maintenance: { lastServiceAt: new Date("2026-02-04T07:00:00Z"), mileageKm: 15400 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000005",
    vin: "WDB55555555555555",
    brand: "Mercedes-Benz",
    model: "E200",
    class: "BUSINESS",
    status: "AVAILABLE",
    pricePerDay: 140,
    createdAt: "2026-02-05T09:00:00Z",
    features: ["air_conditioning", "leather", "navigation"],
    maintenance: { lastServiceAt: new Date("2026-02-05T07:00:00Z"), mileageKm: 28900 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000006",
    vin: "WDB66666666666666",
    brand: "Kia",
    model: "Rio",
    class: "ECONOMY",
    status: "AVAILABLE",
    pricePerDay: 49,
    createdAt: "2026-02-06T09:00:00Z",
    features: ["air_conditioning", "usb"],
    maintenance: { lastServiceAt: new Date("2026-02-06T07:00:00Z"), mileageKm: 19800 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000007",
    vin: "WDB77777777777777",
    brand: "Audi",
    model: "A6",
    class: "LUXURY",
    status: "AVAILABLE",
    pricePerDay: 180,
    createdAt: "2026-02-07T09:00:00Z",
    features: ["air_conditioning", "quattro", "massage_seats"],
    maintenance: { lastServiceAt: new Date("2026-02-07T07:00:00Z"), mileageKm: 24010 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000008",
    vin: "WDB88888888888888",
    brand: "Skoda",
    model: "Octavia",
    class: "COMFORT",
    status: "AVAILABLE",
    pricePerDay: 68,
    createdAt: "2026-02-08T09:00:00Z",
    features: ["air_conditioning", "heated_seats"],
    maintenance: { lastServiceAt: new Date("2026-02-08T07:00:00Z"), mileageKm: 17340 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000009",
    vin: "WDB99999999999999",
    brand: "Volkswagen",
    model: "Tiguan",
    class: "SUV",
    status: "AVAILABLE",
    pricePerDay: 88,
    createdAt: "2026-02-09T09:00:00Z",
    features: ["air_conditioning", "awd", "roof_box"],
    maintenance: { lastServiceAt: new Date("2026-02-09T07:00:00Z"), mileageKm: 26750 }
  },
  {
    _id: "10000000-0000-0000-0000-000000000010",
    vin: "WDBAAAAAAA1111111",
    brand: "Lexus",
    model: "RX350",
    class: "LUXURY",
    status: "MAINTENANCE",
    pricePerDay: 165,
    createdAt: "2026-02-10T09:00:00Z",
    features: ["air_conditioning", "awd", "panoramic_roof"],
    maintenance: { lastServiceAt: new Date("2026-02-10T07:00:00Z"), mileageKm: 30210 }
  }
];

const rentalStatusHistory = (status, changedAt) => [
  { status: "ACTIVE", changedAt, actor: "system" },
  ...(status === "ACTIVE" ? [] : [{ status, changedAt, actor: "system" }])
];

const userSnapshot = (user) => ({
  id: user._id,
  login: user.login,
  firstName: user.firstName,
  lastName: user.lastName,
  email: user.email,
  phone: user.phone,
  driverLicenseNumber: user.driverLicenseNumber,
  role: user.role,
  createdAt: user.createdAt
});

const carSnapshot = (car) => ({
  id: car._id,
  vin: car.vin,
  brand: car.brand,
  model: car.model,
  class: car.class,
  status: car.status === "IN_RENT" ? "AVAILABLE" : car.status,
  pricePerDay: car.pricePerDay,
  createdAt: car.createdAt
});

const rentals = [
  {
    _id: "20000000-0000-0000-0000-000000000001",
    userId: users[0]._id,
    carId: cars[0]._id,
    userSnapshot: userSnapshot(users[0]),
    carSnapshot: carSnapshot(cars[0]),
    startAt: "2026-04-10T09:00:00Z",
    endAt: "2026-04-14T09:00:00Z",
    status: "ACTIVE",
    priceTotal: 220,
    createdAt: "2026-04-10T08:40:00Z",
    paymentReference: "pay-seed-001",
    statusHistory: rentalStatusHistory("ACTIVE", "2026-04-10T08:40:00Z"),
    audit: { seededAt: new Date("2026-04-10T08:40:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000002",
    userId: users[1]._id,
    carId: cars[1]._id,
    userSnapshot: userSnapshot(users[1]),
    carSnapshot: carSnapshot(cars[1]),
    startAt: "2026-04-11T10:00:00Z",
    endAt: "2026-04-15T10:00:00Z",
    status: "ACTIVE",
    priceTotal: 288,
    createdAt: "2026-04-11T09:45:00Z",
    paymentReference: "pay-seed-002",
    statusHistory: rentalStatusHistory("ACTIVE", "2026-04-11T09:45:00Z"),
    audit: { seededAt: new Date("2026-04-11T09:45:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000003",
    userId: users[2]._id,
    carId: cars[2]._id,
    userSnapshot: userSnapshot(users[2]),
    carSnapshot: carSnapshot(cars[2]),
    startAt: "2026-04-12T11:00:00Z",
    endAt: "2026-04-16T11:00:00Z",
    status: "ACTIVE",
    priceTotal: 480,
    createdAt: "2026-04-12T10:50:00Z",
    paymentReference: "pay-seed-003",
    statusHistory: rentalStatusHistory("ACTIVE", "2026-04-12T10:50:00Z"),
    audit: { seededAt: new Date("2026-04-12T10:50:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000004",
    userId: users[3]._id,
    carId: cars[3]._id,
    userSnapshot: userSnapshot(users[3]),
    carSnapshot: carSnapshot(cars[3]),
    startAt: "2026-04-13T08:00:00Z",
    endAt: "2026-04-18T08:00:00Z",
    status: "ACTIVE",
    priceTotal: 475,
    createdAt: "2026-04-13T07:35:00Z",
    paymentReference: "pay-seed-004",
    statusHistory: rentalStatusHistory("ACTIVE", "2026-04-13T07:35:00Z"),
    audit: { seededAt: new Date("2026-04-13T07:35:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000005",
    userId: users[4]._id,
    carId: cars[4]._id,
    userSnapshot: userSnapshot(users[4]),
    carSnapshot: carSnapshot(cars[4]),
    startAt: "2026-03-01T09:00:00Z",
    endAt: "2026-03-05T09:00:00Z",
    status: "COMPLETED",
    priceTotal: 560,
    createdAt: "2026-03-01T08:20:00Z",
    closedAt: "2026-03-05T09:05:00Z",
    paymentReference: "pay-seed-005",
    statusHistory: rentalStatusHistory("COMPLETED", "2026-03-05T09:05:00Z"),
    audit: { seededAt: new Date("2026-03-05T09:05:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000006",
    userId: users[5]._id,
    carId: cars[5]._id,
    userSnapshot: userSnapshot(users[5]),
    carSnapshot: carSnapshot(cars[5]),
    startAt: "2026-03-03T12:00:00Z",
    endAt: "2026-03-06T12:00:00Z",
    status: "COMPLETED",
    priceTotal: 147,
    createdAt: "2026-03-03T11:30:00Z",
    closedAt: "2026-03-06T12:05:00Z",
    paymentReference: "pay-seed-006",
    statusHistory: rentalStatusHistory("COMPLETED", "2026-03-06T12:05:00Z"),
    audit: { seededAt: new Date("2026-03-06T12:05:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000007",
    userId: users[6]._id,
    carId: cars[6]._id,
    userSnapshot: userSnapshot(users[6]),
    carSnapshot: carSnapshot(cars[6]),
    startAt: "2026-03-07T07:00:00Z",
    endAt: "2026-03-10T07:00:00Z",
    status: "COMPLETED",
    priceTotal: 540,
    createdAt: "2026-03-07T06:20:00Z",
    closedAt: "2026-03-10T07:15:00Z",
    paymentReference: "pay-seed-007",
    statusHistory: rentalStatusHistory("COMPLETED", "2026-03-10T07:15:00Z"),
    audit: { seededAt: new Date("2026-03-10T07:15:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000008",
    userId: users[7]._id,
    carId: cars[7]._id,
    userSnapshot: userSnapshot(users[7]),
    carSnapshot: carSnapshot(cars[7]),
    startAt: "2026-03-08T10:00:00Z",
    endAt: "2026-03-12T10:00:00Z",
    status: "COMPLETED",
    priceTotal: 272,
    createdAt: "2026-03-08T09:10:00Z",
    closedAt: "2026-03-12T10:10:00Z",
    paymentReference: "pay-seed-008",
    statusHistory: rentalStatusHistory("COMPLETED", "2026-03-12T10:10:00Z"),
    audit: { seededAt: new Date("2026-03-12T10:10:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000009",
    userId: users[8]._id,
    carId: cars[8]._id,
    userSnapshot: userSnapshot(users[8]),
    carSnapshot: carSnapshot(cars[8]),
    startAt: "2026-03-09T14:00:00Z",
    endAt: "2026-03-11T14:00:00Z",
    status: "CANCELLED",
    priceTotal: 176,
    createdAt: "2026-03-09T13:00:00Z",
    closedAt: "2026-03-09T18:00:00Z",
    paymentReference: "pay-seed-009",
    statusHistory: rentalStatusHistory("CANCELLED", "2026-03-09T18:00:00Z"),
    audit: { seededAt: new Date("2026-03-09T18:00:00Z") }
  },
  {
    _id: "20000000-0000-0000-0000-000000000010",
    userId: users[9]._id,
    carId: cars[9]._id,
    userSnapshot: userSnapshot(users[9]),
    carSnapshot: carSnapshot(cars[9]),
    startAt: "2026-03-10T16:00:00Z",
    endAt: "2026-03-13T16:00:00Z",
    status: "CANCELLED",
    priceTotal: 495,
    createdAt: "2026-03-10T15:30:00Z",
    closedAt: "2026-03-10T20:00:00Z",
    paymentReference: "pay-seed-010",
    statusHistory: rentalStatusHistory("CANCELLED", "2026-03-10T20:00:00Z"),
    audit: { seededAt: new Date("2026-03-10T20:00:00Z") }
  }
];

const outboxEvents = [
  ["30000000-0000-0000-0000-000000000001", "20000000-0000-0000-0000-000000000001", "RentalCreated", "2026-04-10T08:40:00Z", new Date("2026-04-10T08:40:00Z")],
  ["30000000-0000-0000-0000-000000000002", "20000000-0000-0000-0000-000000000002", "RentalCreated", "2026-04-11T09:45:00Z", new Date("2026-04-11T09:45:00Z")],
  ["30000000-0000-0000-0000-000000000003", "20000000-0000-0000-0000-000000000003", "RentalCreated", "2026-04-12T10:50:00Z", new Date("2026-04-12T10:50:00Z")],
  ["30000000-0000-0000-0000-000000000004", "20000000-0000-0000-0000-000000000004", "RentalCreated", "2026-04-13T07:35:00Z", new Date("2026-04-13T07:35:00Z")],
  ["30000000-0000-0000-0000-000000000005", "20000000-0000-0000-0000-000000000005", "RentalCreated", "2026-03-01T08:20:00Z", new Date("2026-03-01T08:20:00Z")],
  ["30000000-0000-0000-0000-000000000006", "20000000-0000-0000-0000-000000000006", "RentalCreated", "2026-03-03T11:30:00Z", new Date("2026-03-03T11:30:00Z")],
  ["30000000-0000-0000-0000-000000000007", "20000000-0000-0000-0000-000000000007", "RentalCreated", "2026-03-07T06:20:00Z", new Date("2026-03-07T06:20:00Z")],
  ["30000000-0000-0000-0000-000000000008", "20000000-0000-0000-0000-000000000008", "RentalCreated", "2026-03-08T09:10:00Z", new Date("2026-03-08T09:10:00Z")],
  ["30000000-0000-0000-0000-000000000009", "20000000-0000-0000-0000-000000000009", "RentalCreated", "2026-03-09T13:00:00Z", new Date("2026-03-09T13:00:00Z")],
  ["30000000-0000-0000-0000-000000000010", "20000000-0000-0000-0000-000000000010", "RentalCreated", "2026-03-10T15:30:00Z", new Date("2026-03-10T15:30:00Z")],
  ["30000000-0000-0000-0000-000000000011", "20000000-0000-0000-0000-000000000005", "RentalCompleted", "2026-03-05T09:05:00Z", new Date("2026-03-05T09:05:00Z")],
  ["30000000-0000-0000-0000-000000000012", "20000000-0000-0000-0000-000000000006", "RentalCompleted", "2026-03-06T12:05:00Z", new Date("2026-03-06T12:05:00Z")],
  ["30000000-0000-0000-0000-000000000013", "20000000-0000-0000-0000-000000000007", "RentalCompleted", "2026-03-10T07:15:00Z", new Date("2026-03-10T07:15:00Z")],
  ["30000000-0000-0000-0000-000000000014", "20000000-0000-0000-0000-000000000008", "RentalCompleted", "2026-03-12T10:10:00Z", new Date("2026-03-12T10:10:00Z")]
].map(([id, aggregateId, eventType, createdAt, emittedAt]) => ({
  _id: id,
  aggregateType: "rental",
  aggregateId,
  eventType,
  payload: {
    eventId: `evt-${id.slice(-3)}`,
    rentalId: aggregateId,
    status: eventType === "RentalCompleted" ? "COMPLETED" : "ACTIVE",
    createdAt
  },
  createdAt,
  meta: { emittedAt }
}));

db.users.deleteMany({});
db.cars.deleteMany({});
db.rentals.deleteMany({});
db.outbox_events.deleteMany({});

db.users.insertMany(users);
db.cars.insertMany(cars);
db.rentals.insertMany(rentals);
db.outbox_events.insertMany(outboxEvents);

print(`Seeded users: ${db.users.countDocuments()}`);
print(`Seeded cars: ${db.cars.countDocuments()}`);
print(`Seeded rentals: ${db.rentals.countDocuments()}`);
print(`Seeded outbox_events: ${db.outbox_events.countDocuments()}`);
