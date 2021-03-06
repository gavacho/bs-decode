open Jest;
open Expect;
open Decode.ParseError;
open Belt.Result;
module D = Decode.AsResult.OfParseError;

let ((<$>), (<*>), (<|>), recoverWith, mapErr) =
  D.R.(Infix.(<$>), Infix.(<*>), Infix.(<|>), recoverWith, mapErr);

let (
  decodeString,
  decodeFloat,
  decodeInt,
  decodeArray,
  decodeList,
  decodeAt,
  decodeField,
  decodeFieldWithFallback,
  decodeOptionalField,
  decodeTuple,
  optional
) = D.(decodeString, decodeFloat, decodeInt, decodeArray, decodeList, decodeAt, decodeField, decodeFieldWithFallback, decodeOptionalField, decodeTuple, decodeOpt);

/**
 * Sample module used to decode json into a record type
 */
module User = {
  type t = { name: string, age: int };
  let make = (name, age) => { name, age };

  let jsonSample = Js.Dict.fromList([
    ("name", Js.Json.string("Foo")),
    ("age", Js.Json.number(30.))
  ]) |> Js.Json.object_;

  let decode = json => make
    <$> decodeField("name", decodeString, json)
    <*> decodeField("age", decodeInt, json);
};

module Parent = {
  type t = { user: User.t, other: float };
  let make = (user, other) => { user, other };
  let decode = obj => make
    <$> decodeField("user", User.decode, obj)
    <*> Ok(3.14) ;
};

describe("Test value decoders", () => {
  let jsonString = Js.Json.string("Foo");
  let jsonFloat = Js.Json.number(1.4);
  let jsonInt: Js.Json.t = [%bs.raw {| 4 |}];
  let jsonNull = Js.Json.null;

  test("String succeeds on string", () => expect(decodeString(jsonString)) |> toEqual(Ok("Foo")));
  test("String fails on number", () => expect(decodeString(jsonFloat)) |> toEqual(Error(Val(`ExpectedString, jsonFloat))));
  test("String fails on null", () => expect(decodeString(jsonNull)) |> toEqual(Error(Val(`ExpectedString, jsonNull))));

  test("Float succeeds on float", () => expect(decodeFloat(jsonFloat)) |> toEqual(Ok(1.4)));
  test("Float succeeds on int", () => expect(decodeFloat(jsonInt)) |> toEqual(Ok(4.0)));
  test("Float fails on string", () => expect(decodeFloat(jsonString)) |> toEqual(Error(Val(`ExpectedNumber, jsonString))));
  test("Float fails on null", () => expect(decodeFloat(jsonNull)) |> toEqual(Error(Val(`ExpectedNumber, jsonNull))));

  test("Int succeeds on int", () => expect(decodeInt(jsonInt)) |> toEqual(Ok(4)));
  test("Int fails on float", () => expect(decodeInt(jsonFloat)) |> toEqual(Error(Val(`ExpectedInt, jsonFloat))));
  test("Int fails on string", () => expect(decodeInt(jsonString)) |> toEqual(Error(Val(`ExpectedNumber, jsonString))));
  test("Int fails on null", () => expect(decodeInt(jsonNull)) |> toEqual(Error(Val(`ExpectedNumber, jsonNull))));
});

describe("Test array decoders", () => {
  let jsonArray = Js.Json.array([| Js.Json.string("a"), Js.Json.string("b"), Js.Json.string("c") |]);
  let jsonEmptyArray = Js.Json.array([||]);
  let jsonString = Js.Json.string("Foo");
  let jsonArrayMixed = Js.Json.array([| Js.Json.string("a"), Js.Json.string("b"), Js.Json.number(3.5) |]);

  let numError = str => Val(`ExpectedNumber, Js.Json.string(str));

  let decodeErrs = NonEmptyList.cons(
    (0, numError("a")), NonEmptyList.cons(
    (1, numError("b")), NonEmptyList.pure(
    (2, numError("c"))
  )));

  test("Array succeeds on array of string", () => expect(decodeArray(decodeString, jsonArray)) |> toEqual(Ok([| "a", "b", "c" |])));
  test("Array string succeeds on empty array", () => expect(decodeArray(decodeString, jsonEmptyArray)) |> toEqual(Ok([||])));
  test("Array int succeeds on empty array", () => expect(decodeArray(decodeInt, jsonEmptyArray)) |> toEqual(Ok([||])));
  test("Array fails on string", () => expect(decodeArray(decodeString, jsonString)) |> toEqual(Error(Val(`ExpectedArray, jsonString))));
  test("Array inner decode int fails on array of string", () => expect(decodeArray(decodeInt, jsonArray)) |> toEqual(Error(Arr(decodeErrs))));
  test("Array fails on mixed array", () =>
    expect(decodeArray(decodeString, jsonArrayMixed)) |> toEqual(Error(Arr(NonEmptyList.pure((2, Val(`ExpectedString, Js.Json.number(3.5)))))))
  );

  test("List succeeds on JSON array of string", () => expect(decodeList(decodeString, jsonArray)) |> toEqual(Ok(["a", "b", "c"])));
  test("List fails on JSON null", () => expect(decodeList(decodeString, Js.Json.null)) |> toEqual(Error(Val(`ExpectedArray, Js.Json.null))));
  test("List inner decode int fails on array of string", () => expect(decodeList(decodeInt, jsonArray)) |> toEqual(Error(Arr(decodeErrs))));
});

describe("Test record field decoders", () => {
  let (string, object_) = Js.Json.(string, object_);
  let (singleton, cons) = NonEmptyList.(pure, cons);
  let obj = User.jsonSample;

  let parentObj = Js.Dict.fromList([("user", obj)]) |> object_;
  let nameAsIntError = InvalidField(Val(`ExpectedNumber, string("Foo")));
  let decoded = User.decode(obj);

  let decodeFail = User.make
    <$> decodeField("missing", decodeString, obj)
    <*> decodeField("name", decodeInt, obj);

  let decodeErrors = Error(Obj(
    cons(("missing", MissingField), singleton(("name", nameAsIntError)))
  ));

  test("String field succeeds", () => expect(decodeField("name", decodeString, obj)) |> toEqual(Ok("Foo")));
  test("Int field succeeds", () => expect(decodeField("age", decodeInt, obj)) |> toEqual(Ok(30)));
  test("Field fails when missing", () => expect(decodeField("blah", decodeInt, obj)) |> toEqual(Error(objPure("blah", MissingField))));
  test("Field fails on wrong type", () => expect(decodeField("name", decodeInt, obj)) |> toEqual(Error(objPure("name", nameAsIntError))));
  test("Decode all fields of user record into User", () => expect(decoded) |> toEqual(Ok(User.make("Foo", 30))));
  test("Decode as user with incorrect fields fails", () => expect(decodeFail) |> toEqual(decodeErrors));
  test("Decode parent record with child record field", () => expect(Parent.decode(parentObj)) |> toEqual(Ok(Parent.make(User.make("Foo", 30), 3.14))));
});

describe("Test value and field recovery from failed parse", () => {
  let jsonNumber = Js.Json.number(3.14);
  let successParse = decodeFloat(jsonNumber);
  let failedParse = decodeString(jsonNumber);

  let invalidUserObj = Js.Dict.fromList([
    ("name", jsonNumber),
    ("age", Js.Json.number(30.))
  ]) |> Js.Json.object_;

  let objRecovery = User.make
    <$> decodeFieldWithFallback("name", decodeString, "Foo", invalidUserObj)
    <*> decodeField("age", decodeInt, invalidUserObj);

  test("Failed parse can be recovered with literal value", () => expect(failedParse |> recoverWith("foo")) |> toEqual(Ok("foo")));
  test("Successful parse is unaffected by recovery", () => expect(successParse |> recoverWith(1.0)) |> toEqual(Ok(3.14)));
  test("Bad field recovers with fallback", () => expect(objRecovery) |> toEqual(Ok(User.make("Foo", 30))));
});

describe("Test optionally empty fields and values", () => {
  let jsonNull = Js.Json.null;
  let jsonString = Js.Json.string("Foo");
  let jsonInt = Js.Json.number(3.);

  let optStr = optional(decodeString);

  let obj = Js.Dict.fromList([
    ("i", jsonInt),
    ("s", jsonString),
    ("n", jsonNull)
  ]) |> Js.Json.object_;

  let optFieldString = decodeOptionalField("s", decodeString, obj);
  let reqFieldOptValueString = decodeField("s", optStr, obj);
  let optFieldInt = decodeOptionalField("x", decodeInt, obj);
  let optValueInt = decodeField("x", optional(decodeInt), obj);
  let nullFieldInt = decodeOptionalField("n", decodeInt, obj);
  let nullValueInt = decodeField("n", optional(decodeInt), obj);

  test("Present value parses as Some string", () => expect(optStr(jsonString)) |> toEqual(Ok(Some("Foo"))));
  test("Null value parses as None", () => expect(optStr(jsonNull)) |> toEqual(Ok(None)));
  test("Present value of incorrect type is still an Error", () => expect(optStr(jsonInt)) |> toEqual(Error(Val(`ExpectedString, jsonInt))));

  test("Present field and value is Some when field is optional", () => expect(optFieldString) |> toEqual(Ok(Some("Foo"))));
  test("Present field and value is Some when value is optional", () => expect(reqFieldOptValueString) |> toEqual(Ok(Some("Foo"))));
  test("If int field is missing, it parses as None if field is optional", () => expect(optFieldInt) |> toEqual(Ok(None)));
  test("If int field is missing, it parses as Error if only value is optional", () => expect(optValueInt) |> toEqual(Error(objPure("x", MissingField))));
  test("Present field with null value parses as None for optional field", () => expect(nullFieldInt) |> toEqual(Ok(None)));
  test("Present field with null value parses as None for optional value", () => expect(nullValueInt) |> toEqual(Ok(None)));
  test("Trying to parse string field as int is Error, not None", () => expect(decodeOptionalField("s", decodeInt, obj)) |> toEqual(Error(Val(`ExpectedNumber, jsonString))));
});
