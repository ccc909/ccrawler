# Stage 1: Build the frontend
FROM node:alpine AS frontend-build

WORKDIR /app/frontend

# Install Git
RUN apk add --no-cache git

# Copy frontend source code
COPY /frontend .

# Build frontend
RUN npm install
RUN npm run build

# Stage 2: Build the backend
FROM ubuntu:22.04 AS backend-build

WORKDIR /app/backend

# Install Git, build tools, and necessary libraries
RUN apt-get update && apt-get install -y git wget make gcc g++ libssl-dev libmbedtls-dev libcurl4-openssl-dev cmake && apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy backend source code
COPY /backend .

# Build C++ backend
RUN cmake -B build -S .
RUN cmake --build build --config Debug

# Stage 3: Deploy using nginx, maybe could merge with 2 but for now it's separate
FROM ubuntu/nginx

# Install runtime libraries for the C++ backend
RUN apt-get update && apt-get install -y libstdc++6 libcurl4 gdb libssl-dev && apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy built frontend from the frontend build stage
COPY --from=frontend-build /app/frontend/dist/project/browser /var/www/html

# Copy the built backend executable and libraries
COPY --from=backend-build /app/backend/build/cpr_example /usr/local/bin/
COPY --from=backend-build /usr/lib/x86_64-linux-gnu/libssl.so.3 /usr/local/lib/
COPY --from=backend-build /usr/lib/x86_64-linux-gnu/libcrypto.so.3 /usr/local/lib/
COPY --from=backend-build /app/backend/build/libcpr.so.1 /usr/local/lib/
COPY --from=backend-build /app/backend/build/libcurl.so /usr/local/lib/

# Ensure the dynamic linker can find the libraries
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/lib:/lib

EXPOSE 80

# Ensure the backend executable has execute permissions
RUN chmod +x /usr/local/bin/cpr_example

# Entry point to run both the backend and Nginx
ENTRYPOINT ["/bin/sh", "-c", "/usr/local/bin/cpr_example & nginx -g 'daemon off;'"]
