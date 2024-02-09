CREATE TABLE IF NOT EXISTS todo_list_table (
    id SERIAL PRIMARY KEY,
    user_ip VARCHAR(64) NOT NULL,
    note_text TEXT NOT NULL,
    note_status BOOLEAN
);