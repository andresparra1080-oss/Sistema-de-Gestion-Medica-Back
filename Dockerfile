FROM ubuntu:22.04

WORKDIR /app

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        g++ \
        ca-certificates \
        git \
        python3 \
        sqlite3 \
        libsqlite3-dev \
        libssl-dev \
        libasio-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

RUN mkdir -p /data

EXPOSE 8080

CMD sh -c "printf 'PORT=%s\nDB_PATH=%s\nDB_NAME=%s\n' \"${PORT:-8080}\" \"/data/heartmed_db.sqlite3\" \"heartmed_db\" > /app/.env && exec /app/build/heartmed-backend"
