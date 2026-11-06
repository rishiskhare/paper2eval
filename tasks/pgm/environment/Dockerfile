FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852 AS dev-workloads
RUN apt-get update && apt-get install -y --no-install-recommends python3 && rm -rf /var/lib/apt/lists/*
COPY evaluator/gen_workloads.py evaluator/dev_spec.json /gen/
RUN python3 /gen/gen_workloads.py /gen/dev_spec.json /out

FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852
RUN apt-get update && apt-get install -y --no-install-recommends g++ make cmake python3 patch ca-certificates poppler-utils \
    && rm -rf /var/lib/apt/lists/*
COPY repo/ /home/dev/PGM-index/
COPY paper/paper.pdf /home/dev/papers/pgm-index.pdf
COPY repo/ /opt/pgm-tools/upstream/
COPY harness/harness.cpp evaluator/pgm_check.py evaluator/check.py evaluator/check.sh evaluator/upstream_counts.json /opt/pgm-tools/
COPY --from=dev-workloads /out/ /opt/pgm-tools/data/
RUN chmod -R a-w /opt/pgm-tools && ln -s /opt/pgm-tools/check.sh /usr/local/bin/pgm-check
RUN useradd -m -d /home/dev -s /bin/bash dev && chown -R dev:dev /home/dev
USER dev
ENV HOME=/home/dev
WORKDIR /home/dev/PGM-index
