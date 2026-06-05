#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
JAVA="${JAVA:-java}"
TIKA_JAR="${TIKA_JAR:-${ROOT_DIR}/third_party/cache/tika-app-3.3.1.jar}"
WORKER_JAR="${WORKER_JAR:-${ROOT_DIR}/parser/build/parser-worker.jar}"
temporary="$(mktemp -d)"
trap 'rm -rf "${temporary}"' EXIT

cp "${WORKER_JAR}" "${temporary}/parser-worker.jar"
cp "${TIKA_JAR}" "${temporary}/tika-app-3.3.1.jar"
printf '中华人民共和国文档，支持中文全文检索。\\n' >"${temporary}/source.txt"
printf '中文路径解析成功。\\n' >"${temporary}/中文路径.txt"
printf '{"id":1,"input":"%s","output":"%s","maxChars":1000000}\\n' \
  "${temporary}/source.txt" "${temporary}/output.txt" |
  (cd "${temporary}" && "${JAVA}" -jar parser-worker.jar) >"${temporary}/response.jsonl"
grep -q '"status":"ok"' "${temporary}/response.jsonl"
grep -q '中华人民共和国文档' "${temporary}/output.txt"
printf '{"id":2,"input":"%s","output":"%s","maxChars":1000000}\\n' \
  "${temporary}/中文路径.txt" "${temporary}/中文输出.txt" |
  (cd "${temporary}" && LANG=C.UTF-8 LC_ALL=C.UTF-8 "${JAVA}" \
    -Dfile.encoding=UTF-8 -jar parser-worker.jar) >"${temporary}/response-chinese-path.jsonl"
grep -q '"status":"ok"' "${temporary}/response-chinese-path.jsonl"
grep -q '中文路径解析成功' "${temporary}/中文输出.txt"
echo "Parser Worker smoke test passed."
