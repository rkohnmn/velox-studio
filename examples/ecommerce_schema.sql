CREATE TABLE customers (
    id BIGINT PRIMARY KEY,
    email VARCHAR(255) NOT NULL UNIQUE,
    created_at TIMESTAMP NOT NULL
);

CREATE TABLE orders (
    id BIGINT PRIMARY KEY,
    customer_id BIGINT NOT NULL REFERENCES customers(id),
    total_price FLOAT NOT NULL,
    tag1 VARCHAR(64),
    tag2 VARCHAR(64),
    created_at TIMESTAMP NOT NULL
);

CREATE TABLE shipments (
    id BIGINT PRIMARY KEY,
    order_id BIGINT NOT NULL REFERENCES orders(id),
    carrier VARCHAR(64) NOT NULL,
    shipped_at TIMESTAMP
);
