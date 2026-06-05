package com.wensousou.parser;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.apache.tika.Tika;

public final class ParserWorker {
  private static final Pattern FIELD_PATTERN =
      Pattern.compile("\"([^\"]+)\"\\s*:\\s*(\"(?:\\\\.|[^\"])*\"|-?\\d+|true|false|null)");

  private final Tika tika = new Tika();

  public static void main(String[] args) throws Exception {
    new ParserWorker().run();
  }

  private void run() throws IOException {
    try (BufferedReader reader =
             Files.newBufferedReader(Paths.get("/dev/stdin"), StandardCharsets.UTF_8);
         BufferedWriter writer =
             new BufferedWriter(new OutputStreamWriter(System.out, StandardCharsets.UTF_8))) {
      String line;
      while ((line = reader.readLine()) != null) {
        if (line.trim().isEmpty()) continue;
        writer.write(handle(line));
        writer.newLine();
        writer.flush();
      }
    }
  }

  private String handle(String line) {
    long id = 0;
    try {
      Map<String, String> request = parseFlatJson(line);
      id = longValue(request.get("id"), 0);
      String input = stringValue(request.get("input"));
      String output = stringValue(request.get("output"));
      int maxChars = (int) Math.max(1, longValue(request.get("maxChars"), 1000000));
      if (input.isEmpty() || output.isEmpty()) {
        return response(id, "failed", 0, false, "缺少 input 或 output。");
      }

      String text = tika.parseToString(Paths.get(input));
      if (text == null) text = "";
      boolean truncated = text.length() > maxChars;
      if (truncated) text = text.substring(0, maxChars);
      Files.write(Paths.get(output), text.getBytes(StandardCharsets.UTF_8));
      return response(id, "ok", text.length(), truncated, "");
    } catch (Throwable throwable) {
      return response(id, "failed", 0, false, throwable.getMessage());
    }
  }

  private static Map<String, String> parseFlatJson(String line) {
    Map<String, String> values = new LinkedHashMap<>();
    Matcher matcher = FIELD_PATTERN.matcher(line);
    while (matcher.find()) {
      values.put(matcher.group(1), matcher.group(2));
    }
    return values;
  }

  private static long longValue(String value, long fallback) {
    if (value == null || value.isEmpty()) return fallback;
    try {
      return Long.parseLong(value);
    } catch (NumberFormatException ignored) {
      return fallback;
    }
  }

  private static String stringValue(String value) {
    if (value == null || value.length() < 2 || value.charAt(0) != '"') return "";
    return unescape(value.substring(1, value.length() - 1));
  }

  private static String unescape(String value) {
    StringBuilder builder = new StringBuilder(value.length());
    for (int index = 0; index < value.length(); ++index) {
      char current = value.charAt(index);
      if (current != '\\' || index + 1 >= value.length()) {
        builder.append(current);
        continue;
      }
      char escaped = value.charAt(++index);
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          builder.append(escaped);
          break;
        case 'b':
          builder.append('\b');
          break;
        case 'f':
          builder.append('\f');
          break;
        case 'n':
          builder.append('\n');
          break;
        case 'r':
          builder.append('\r');
          break;
        case 't':
          builder.append('\t');
          break;
        case 'u':
          if (index + 4 < value.length()) {
            builder.append((char) Integer.parseInt(value.substring(index + 1, index + 5), 16));
            index += 4;
          }
          break;
        default:
          builder.append(escaped);
      }
    }
    return builder.toString();
  }

  private static String response(long id, String status, int chars, boolean truncated,
                                 String error) {
    return "{\"id\":" + id
        + ",\"status\":\"" + escape(status)
        + "\",\"chars\":" + chars
        + ",\"truncated\":" + truncated
        + ",\"error\":\"" + escape(error == null ? "" : error) + "\"}";
  }

  private static String escape(String value) {
    StringBuilder builder = new StringBuilder(value.length());
    for (int index = 0; index < value.length(); ++index) {
      char current = value.charAt(index);
      switch (current) {
        case '"':
          builder.append("\\\"");
          break;
        case '\\':
          builder.append("\\\\");
          break;
        case '\b':
          builder.append("\\b");
          break;
        case '\f':
          builder.append("\\f");
          break;
        case '\n':
          builder.append("\\n");
          break;
        case '\r':
          builder.append("\\r");
          break;
        case '\t':
          builder.append("\\t");
          break;
        default:
          if (current < 0x20) {
            builder.append(String.format("\\u%04x", (int) current));
          } else {
            builder.append(current);
          }
      }
    }
    return builder.toString();
  }
}
