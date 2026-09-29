#include <shadowlist-core/host/ElementSizeSpec.hpp>
#include <shadowlist-core/host/JsonValue.hpp>

namespace azimgd::shadowlist {

namespace {

double optionalDouble(const JsonValue& object, const char* name, double fallback) {
  const JsonValue* value = object.find(name);
  return value != nullptr && value->isNumber() ? value->asDouble() : fallback;
}

std::string optionalString(const JsonValue& object, const char* name) {
  const JsonValue* value = object.find(name);
  return value != nullptr && value->isString() ? value->getString() : std::string{};
}

}

std::vector<ElementSizeSpec> parseElementSizeSpecs(const std::string& json) {
  std::vector<ElementSizeSpec> specs;
  if (json.empty()) {
    return specs;
  }
  std::optional<JsonValue> parsed = JsonValue::parse(json);
  if (!parsed || !parsed->isArray()) {
    return specs;
  }

  specs.reserve(parsed->size());
  for (const JsonValue& entry : parsed->getArray()) {
    if (!entry.isObject()) {
      continue;
    }
    ElementSizeSpec spec;
    spec.key = optionalString(entry, "key");
    if (spec.key.empty()) {
      continue;
    }
    spec.text = optionalString(entry, "text");
    spec.fontFamily = optionalString(entry, "fontFamily");
    spec.fontWeight = optionalString(entry, "fontWeight");
    // fontWeight can also be a number like 700, so turn it into a string.
    if (const JsonValue* fontWeight = entry.find("fontWeight"); fontWeight != nullptr && fontWeight->isNumber()) {
      spec.fontWeight = std::to_string(static_cast<int>(fontWeight->asDouble()));
    }
    spec.fontStyle = optionalString(entry, "fontStyle");
    spec.fontSize = optionalDouble(entry, "fontSize", ElementSizeSpec::DEFAULT_FONT_SIZE);
    spec.lineHeight = optionalDouble(entry, "lineHeight", std::numeric_limits<double>::quiet_NaN());
    spec.letterSpacing = optionalDouble(entry, "letterSpacing", std::numeric_limits<double>::quiet_NaN());
    spec.numberOfLines = static_cast<int>(optionalDouble(entry, "numberOfLines", 0.0));
    spec.insetWidth = optionalDouble(entry, "insetWidth", 0.0);
    spec.insetHeight = optionalDouble(entry, "insetHeight", 0.0);
    spec.widthFraction = optionalDouble(entry, "widthFraction", 1.0);
    spec.fixedHeight = optionalDouble(entry, "fixedHeight", std::numeric_limits<double>::quiet_NaN());
    specs.push_back(std::move(spec));
  }
  return specs;
}

}
