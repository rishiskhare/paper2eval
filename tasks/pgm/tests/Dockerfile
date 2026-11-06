# Verifier image: pristine stripped tree, trusted harness, eval.py, held-out spec and reference counts.
FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852
RUN apt-get update && apt-get install -y --no-install-recommends g++ python3 && rm -rf /var/lib/apt/lists/*
COPY repo/ /opt/pgm-verifier/pristine/
COPY harness/harness.cpp evaluator/pgm_check.py evaluator/eval.py evaluator/gen_workloads.py /opt/pgm-verifier/
COPY tests/heldout_spec.json tests/reference_values.json /opt/pgm-verifier/
COPY tests/test.sh /tests/test.sh
RUN chmod +x /tests/test.sh && useradd --system --no-create-home --shell /usr/sbin/nologin grader \
    && chmod -R go-rwx /opt/pgm-verifier /tests
