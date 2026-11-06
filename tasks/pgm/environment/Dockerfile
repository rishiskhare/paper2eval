FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852
RUN apt-get update && apt-get install -y --no-install-recommends g++ make cmake python3 patch ca-certificates poppler-utils \
    && rm -rf /var/lib/apt/lists/*
COPY repo/ /home/dev/PGM-index/
COPY paper/paper.pdf /home/dev/papers/pgm-index.pdf
RUN useradd -m -d /home/dev -s /bin/bash dev && chown -R dev:dev /home/dev
USER dev
ENV HOME=/home/dev
WORKDIR /home/dev/PGM-index
