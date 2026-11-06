# Verifier image: pristine repo, held-out checker and the authors' segment counts.
FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852
RUN apt-get update && apt-get install -y --no-install-recommends g++ && rm -rf /var/lib/apt/lists/*
COPY repo/ /opt/pgm-verifier/pristine/
COPY tests/count.cpp tests/reference_counts.txt /opt/pgm-verifier/
COPY tests/test.sh /tests/test.sh
RUN chmod +x /tests/test.sh && useradd --system --no-create-home --shell /usr/sbin/nologin grader \
    && chmod -R go-rwx /opt/pgm-verifier /tests
