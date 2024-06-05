FROM node:alpine AS build

WORKDIR /app

COPY /frontend .

RUN npm install

RUN npm run build

FROM nginx:alpine AS prod

COPY --from=build /app/dist/project/browser /usr/share/nginx/html

EXPOSE 80
