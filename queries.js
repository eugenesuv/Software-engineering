print("== Create ==");

db.users.insertOne({
  _id: "00000000-0000-0000-0000-000000009999",
  login: "new.customer",
  firstName: "New",
  lastName: "Customer",
  email: "new.customer@example.com",
  phone: "+79009999999",
  driverLicenseNumber: "DL-NEW-999",
  role: "CUSTOMER",
  createdAt: "2026-04-20T10:00:00Z",
  credentials: { salt: "new-salt", hash: "new-hash" },
  profile: { seededAt: new Date("2026-04-20T10:00:00Z"), loyaltyLevel: 1 }
});

db.cars.insertOne({
  _id: "10000000-0000-0000-0000-000000009999",
  vin: "WDBQUERY000000999",
  brand: "Mazda",
  model: "CX-5",
  class: "SUV",
  status: "AVAILABLE",
  pricePerDay: 93,
  createdAt: "2026-04-20T10:05:00Z",
  features: ["air_conditioning", "bluetooth"],
  maintenance: { lastServiceAt: new Date("2026-04-01T08:00:00Z"), mileageKm: 12000 }
});

db.rentals.insertOne({
  _id: "20000000-0000-0000-0000-000000009999",
  userId: "00000000-0000-0000-0000-000000009999",
  carId: "10000000-0000-0000-0000-000000009999",
  userSnapshot: {
    id: "00000000-0000-0000-0000-000000009999",
    login: "new.customer",
    firstName: "New",
    lastName: "Customer",
    email: "new.customer@example.com",
    phone: "+79009999999",
    driverLicenseNumber: "DL-NEW-999",
    role: "CUSTOMER",
    createdAt: "2026-04-20T10:00:00Z"
  },
  carSnapshot: {
    id: "10000000-0000-0000-0000-000000009999",
    vin: "WDBQUERY000000999",
    brand: "Mazda",
    model: "CX-5",
    class: "SUV",
    status: "AVAILABLE",
    pricePerDay: 93,
    createdAt: "2026-04-20T10:05:00Z"
  },
  startAt: "2026-04-21T09:00:00Z",
  endAt: "2026-04-24T09:00:00Z",
  status: "ACTIVE",
  priceTotal: 279,
  createdAt: "2026-04-20T10:10:00Z",
  paymentReference: "pay-query-999",
  statusHistory: [{ status: "ACTIVE", changedAt: "2026-04-20T10:10:00Z", actor: "system" }],
  audit: { seededAt: new Date("2026-04-20T10:10:00Z") }
});

print("== Read ==");

print("User by login ($eq):");
printjson(db.users.findOne({ login: { $eq: "alice.driver" } }));

print("Users with name/surname masks ($and):");
printjson(
  db.users.find({
    $and: [
      { firstName: { $regex: "a", $options: "i" } },
      { lastName: { $regex: "o|e", $options: "i" } }
    ]
  }).sort({ createdAt: 1, _id: 1 }).toArray()
);

print("Cars available and not in maintenance ($and, $ne):");
printjson(
  db.cars.find({
    $and: [
      { status: { $eq: "AVAILABLE" } },
      { status: { $ne: "MAINTENANCE" } }
    ]
  }).sort({ createdAt: 1, _id: 1 }).toArray()
);

print("Cars by classes ($in):");
printjson(db.cars.find({ class: { $in: ["BUSINESS", "SUV"] } }).toArray());

print("Premium cars with price filter ($gt, $lt, $or):");
printjson(
  db.cars.find({
    $and: [
      { pricePerDay: { $gt: 80 } },
      { pricePerDay: { $lt: 170 } },
      { $or: [{ class: "BUSINESS" }, { class: "SUV" }] }
    ]
  }).toArray()
);

print("Active rentals by user:");
printjson(
  db.rentals.find({
    $and: [
      { userId: "00000000-0000-0000-0000-000000000001" },
      { status: "ACTIVE" }
    ]
  }).sort({ createdAt: 1, _id: 1 }).toArray()
);

print("Rental history by user ($or):");
printjson(
  db.rentals.find({
    userId: "00000000-0000-0000-0000-000000000009",
    $or: [{ status: "COMPLETED" }, { status: "CANCELLED" }]
  }).toArray()
);

print("== Update ==");

print("Add feature to a car ($addToSet):");
printjson(
  db.cars.updateOne(
    { _id: "10000000-0000-0000-0000-000000000005" },
    { $addToSet: { features: "wifi_hotspot" } }
  )
);

print("Remove feature from a car ($pull):");
printjson(
  db.cars.updateOne(
    { _id: "10000000-0000-0000-0000-000000000005" },
    { $pull: { features: "wifi_hotspot" } }
  )
);

print("Complete rental and append status history ($set, $push):");
printjson(
  db.rentals.updateOne(
    { _id: "20000000-0000-0000-0000-000000009999", status: "ACTIVE" },
    {
      $set: {
        status: "COMPLETED",
        closedAt: "2026-04-24T09:01:00Z"
      },
      $push: {
        statusHistory: {
          status: "COMPLETED",
          changedAt: "2026-04-24T09:01:00Z",
          actor: "manager"
        }
      }
    }
  )
);

print("Mark car available after completion:");
printjson(
  db.cars.updateOne(
    { _id: "10000000-0000-0000-0000-000000009999" },
    { $set: { status: "AVAILABLE" }, $unset: { activeRentalId: true } }
  )
);

print("== Delete ==");

print("Delete demo outbox event:");
printjson(
  db.outbox_events.deleteOne({
    aggregateId: "20000000-0000-0000-0000-000000009999",
    eventType: "RentalCompleted"
  })
);

print("Delete demo rental:");
printjson(db.rentals.deleteOne({ _id: "20000000-0000-0000-0000-000000009999" }));

print("Delete demo car:");
printjson(db.cars.deleteOne({ _id: "10000000-0000-0000-0000-000000009999" }));

print("Delete demo user:");
printjson(db.users.deleteOne({ _id: "00000000-0000-0000-0000-000000009999" }));

print("== Aggregation ==");

print("Revenue by car class for completed rentals:");
printjson(
  db.rentals.aggregate([
    { $match: { status: "COMPLETED" } },
    {
      $group: {
        _id: "$carSnapshot.class",
        totalRevenue: { $sum: "$priceTotal" },
        rentalsCount: { $sum: 1 }
      }
    },
    {
      $project: {
        _id: 0,
        carClass: "$_id",
        totalRevenue: 1,
        rentalsCount: 1
      }
    },
    { $sort: { totalRevenue: -1, carClass: 1 } }
  ]).toArray()
);
