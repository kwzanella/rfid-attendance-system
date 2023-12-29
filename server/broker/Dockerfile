FROM eclipse-mosquitto:latest

EXPOSE 1883

COPY . .

RUN chmod 0700 /mosquitto/config/pwfile

#CMD ["mosquitto", "-v", "-c", "/mosquitto/config/mosquitto.conf"]
CMD ["mosquitto", "-c", "/mosquitto/config/mosquitto.conf"]
