FROM ubuntu:24.04

RUN apt update -y && apt upgrade -y \
   && apt install -y apache2 \
   nano vim curl net-tools iproute2 iputils-ping socat \
   && apt clean

RUN a2enmod rewrite ssl headers \
    && service apache2 restart || true

WORKDIR /var/www/html

COPY apache-config.conf /etc/apache2/sites-available/000-default.conf
COPY ports.conf /etc/apache2/ports.conf

RUN echo "ServerName vehiclemap.xyz" >> /etc/apache2/apache2.conf

CMD [ "apachectl", "-D", "FOREGROUND" ]
