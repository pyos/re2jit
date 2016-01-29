FIXED_TEST("(x\\g<1>*y)*", ANCHOR_BOTH, "xxxyyy", true, "xxxyyy", GARBAGE_GROUP);
FIXED_TEST("(x\\g<1>*y)*", ANCHOR_BOTH, "xxxyyyxxyyxyxyxxyxyy", true, "xxxyyyxxyyxyxyxxyxyy", GARBAGE_GROUP);
FIXED_TEST("(x\\g<1>*y)*", ANCHOR_BOTH, "xxxyyyxxyyxyxyxxyxyyy", false, "", "");
FIXED_TEST("(x\\g<1>*y)*", ANCHOR_BOTH, "xxxyyyxxyyxyxyxxyxyyx", false, "", "");
FIXED_TEST("(x\\g<1>*y)*", ANCHOR_BOTH, "xxxyyyyxxyyxyxyxxyxyy", false, "", "");

// hehe
// http://davidvgalbraith.com/how-i-fixed-atom/
FIXED_TEST("^\\s*[^\\s()}]+([^()]*\\((?:\\g<1>|[^()]*)\\)[^()]*)*[^()]*\\)[,]?$", ANCHOR_BOTH, "vVar.Type().Name() == \"\" && vVar.Kind() == reflect.Ptr && vVar.Type().Elem().Name() == \"\" && vVar.Type().Elem().Kind() == reflect.Slice", false, "", "");
FIXED_TEST("^\\s*[^\\s()}]+([^()]*\\((?:\\g<1>|[^()]*)\\)[^()]*)*[^()]*\\)[,]?$", ANCHOR_BOTH, "vVar.Type().Name() == \"\" && vVar.Kind() == reflect.Ptr && vVar.Type().Elem().Name() == \"\" && vVar.Type().Elem().Kind() == reflect.Slice)", true, GARBAGE_GROUP, GARBAGE_GROUP);
FIXED_TEST("^\\s*[^\\s()}]+([^()]*\\((?:\\g<1>|[^()]*)\\)[^()]*)*[^()]*\\)[,]?$", ANCHOR_BOTH, "vVar.Type().Name() == \"\" && vVar.Kind() == reflect.Ptr && vVar.Type().Elem().Name() == \"\" && vVar.Type(argument()).Elem().Kind() == reflect.Slice", false, "", "");
FIXED_TEST("^\\s*[^\\s()}]+([^()]*\\((?:\\g<1>|[^()]*)\\)[^()]*)*[^()]*\\)[,]?$", ANCHOR_BOTH, "vVar.Type().Name() == \"\" && vVar.Kind() == reflect.Ptr && vVar.Type().Elem().Name() == \"\" && vVar.Type(argument()).Elem().Kind() == reflect.Slice)", true, GARBAGE_GROUP, GARBAGE_GROUP);
