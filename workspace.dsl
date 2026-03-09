workspace "Homework 01 - Car Rental Management System" "C4 model for car rental management system (variant 21)" {

    model {
        customer = person "Customer" "Клиент, который арендует автомобиль."
        fleetManager = person "Fleet Manager" "Менеджер автопарка, который управляет автомобилями."

        driverLicenseVerification = softwareSystem "Driver License Verification Service" "Проверяет действительность водительского удостоверения." "External System"
        paymentProvider = softwareSystem "Payment Provider" "Выполняет предавторизацию и списание оплаты аренды." "External System"
        notificationProvider = softwareSystem "Email/SMS Provider" "Отправляет пользователю email/SMS уведомления." "External System"
        messageBroker = softwareSystem "Message Broker" "Передает асинхронные события между сервисами." "External System"

        carRentalSystem = softwareSystem "Car Rental Management System" "Система управления арендой автомобилей." {
            webApp = container "Web Application" "Пользовательский интерфейс для клиента и менеджера автопарка." "React SPA" "Web Application"
            apiGateway = container "API Gateway / BFF" "Единая точка входа, аутентификация и маршрутизация к доменным сервисам." "Node.js REST API" "API"
            userService = container "User Service" "Управление пользователями и поиск пользователей." "REST API" "Service"
            fleetService = container "Fleet Service" "Управление автомобилями, доступностью и поиском по классу." "REST API" "Service"
            rentalService = container "Rental Service" "Создание и завершение аренд, получение активных аренд и истории." "REST API" "Service"
            notificationWorker = container "Notification Worker" "Обрабатывает события аренды и отправляет уведомления пользователю." "Async Worker" "Worker"
            database = container "Relational Database" "Хранит данные users, cars, rentals с логическим разделением по схемам user/fleet/rental." "PostgreSQL" "Database"

            !docs docs/system
            !decisions docs/decisions adrtools
        }

        customer -> carRentalSystem "Ищет авто и управляет арендой" "HTTPS"
        fleetManager -> carRentalSystem "Управляет автопарком" "HTTPS"
        carRentalSystem -> driverLicenseVerification "Проверяет водительские удостоверения" "HTTPS/REST"
        carRentalSystem -> paymentProvider "Выполняет предавторизацию и списание оплаты" "HTTPS/REST"
        carRentalSystem -> notificationProvider "Отправляет уведомления клиентам" "HTTPS/REST"
        carRentalSystem -> messageBroker "Публикует и потребляет доменные события" "AMQP/Kafka protocol"

        customer -> webApp "Использует web-интерфейс клиента" "HTTPS"
        fleetManager -> webApp "Использует web-интерфейс менеджера" "HTTPS"
        webApp -> apiGateway "Вызывает backend API" "HTTPS/JSON"

        apiGateway -> userService "Маршрутизирует user-запросы" "HTTPS/REST"
        apiGateway -> fleetService "Маршрутизирует fleet-запросы" "HTTPS/REST"
        apiGateway -> rentalService "Маршрутизирует rental-запросы" "HTTPS/REST"

        rentalService -> userService "Проверяет пользователя перед созданием аренды" "HTTPS/REST"
        rentalService -> fleetService "Проверяет доступность автомобиля" "HTTPS/REST"

        userService -> database "Читает/записывает пользователей (своя схема)" "SQL"
        fleetService -> database "Читает/записывает автомобили (своя схема)" "SQL"
        rentalService -> database "Читает/записывает аренды (своя схема)" "SQL"

        rentalService -> driverLicenseVerification "Валидирует водительское удостоверение" "HTTPS/REST"
        rentalService -> paymentProvider "Предавторизует/списывает оплату аренды" "HTTPS/REST"
        rentalService -> messageBroker "Публикует события RentalCreated/RentalCompleted (transactional outbox)" "Async event publish"
        notificationWorker -> messageBroker "Потребляет события аренд (idempotent consume)" "Async event consume"
        notificationWorker -> notificationProvider "Отправляет email/SMS уведомления" "HTTPS/REST"
    }

    views {
        systemContext carRentalSystem "SystemContext" "C1: Контекст системы аренды автомобилей." {
            include customer
            include fleetManager
            include carRentalSystem
            include driverLicenseVerification
            include paymentProvider
            include notificationProvider
            include messageBroker
            autolayout lr
        }

        container carRentalSystem "Container" "C2: Контейнеры системы и их взаимодействия." {
            include *
            autolayout lr
        }

        dynamic carRentalSystem "CreateRental" "Dynamic: сценарий создания аренды." {
            customer -> webApp "1. Выбирает автомобиль и период аренды" "HTTPS"
            webApp -> apiGateway "2. Отправляет POST /rentals" "HTTPS/JSON"
            apiGateway -> rentalService "3. Передает команду создания аренды" "HTTPS/REST"
            rentalService -> userService "4. Проверяет существование и статус пользователя" "HTTPS/REST"
            rentalService -> fleetService "5. Проверяет доступность автомобиля" "HTTPS/REST"
            rentalService -> driverLicenseVerification "6. Запрашивает проверку водительского удостоверения" "HTTPS/REST"
            rentalService -> paymentProvider "7. Выполняет предавторизацию платежа" "HTTPS/REST"
            rentalService -> database "8. Сохраняет аренду со статусом ACTIVE" "SQL"
            rentalService -> fleetService "9. Обновляет статус автомобиля на IN_RENT" "HTTPS/REST"
            fleetService -> database "10. Фиксирует новый статус автомобиля" "SQL"
            rentalService -> messageBroker "11. Публикует событие RentalCreated (transactional outbox)" "Async event publish"
            apiGateway -> webApp "12. Возвращает подтверждение создания аренды (без ожидания уведомления)" "HTTPS/JSON"
            notificationWorker -> messageBroker "13. Потребляет событие RentalCreated идемпотентно" "Async event consume"
            notificationWorker -> notificationProvider "14. Отправляет уведомление клиенту" "HTTPS/REST"
            autolayout lr
        }

        styles {
            element "Person" {
                background "#0B5D91"
                color "#FFFFFF"
                shape Person
            }
            element "Software System" {
                background "#2E7D32"
                color "#FFFFFF"
            }
            element "External System" {
                background "#616161"
                color "#FFFFFF"
            }
            element "Container" {
                background "#1565C0"
                color "#FFFFFF"
            }
            element "Database" {
                shape Cylinder
                background "#455A64"
                color "#FFFFFF"
            }
            element "Worker" {
                background "#6A1B9A"
                color "#FFFFFF"
            }
            relationship "Relationship" {
                color "#424242"
                thickness 2
            }
        }
    }

    configuration {
        scope softwaresystem
    }
}
