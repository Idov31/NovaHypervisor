#ifndef WPP_DEFINITIONS_H
#define WPP_DEFINITIONS_H

// {e74c1035-77d4-4c5b-9088-77056fae3aa3}

#define WPP_CONTROL_GUIDS                                              \
    WPP_DEFINE_CONTROL_GUID(                                           \
        NovaHypervisorLogger, (e74c1035,77d4,4c5b,9088,77056fae3aa3), \
			WPP_DEFINE_BIT(TRACE_FLAG_DEBUG) \
			WPP_DEFINE_BIT(TRACE_FLAG_INFO) \
			WPP_DEFINE_BIT(TRACE_FLAG_WARNING) \
			WPP_DEFINE_BIT(TRACE_FLAG_ERROR) \
   ) 

//#define WPP_Flags_LEVEL_LOGGER(Flags, level)                                  \
//    WPP_LEVEL_LOGGER(Flags)
//
//#define WPP_Flags_LEVEL_ENABLED(Flags, level)                                 \
//    (WPP_LEVEL_ENABLED(Flags) && \
//    WPP_CONTROL(WPP_BIT_ ## Flags).Level >= level)

//
// DoTraceLevelMessage is a custom macro that adds support for levels to the 
// default DoTraceMessage, which supports only flags. In this version, both
// flags and level are conditions for generating the trace message. 
// The preprocessor is told to recognize the function by using the -func argument
// in the RUN_WPP line on the source file. In the source file you will find
// -func:DoTraceLevelMessage(LEVEL,FLAGS,MSG,...). The conditions for triggering 
// this event in the macro are the Levels defined in evntrace.h and the flags 
// defined above and are evaluated by the macro WPP_LEVEL_FLAGS_ENABLED below. 
// 
#define WPP_LEVEL_FLAGS_LOGGER(level,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(level, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= level)

typedef enum _LoggerLevel {
	Debug = 0,
	Info,
	Warning,
	Error
} LoggerLevel;
//
// Configuration block to scan the enumeration definition LoggerLevel. Used when  
// viewing the trace to display names instead of the integer values that users must decode
//
// begin_wpp config
// CUSTOM_TYPE(state, ItemEnum(_LoggerLevel));
// end_wpp


// MACRO: TRACE_RETURN
// Configuration block that defines trace macro. It uses the PRE/POST macros to include
// code as part of the trace macro expansion. TRACE_MACRO is equivalent to the code below:
//
// {if (Status != STATUS_SUCCESS){  // This is the code in the PRE macro
//     DoTraceMessage(FLAG_ONE, "Function Return = %!STATUS!", Status)
// ;}}                              // This is the code in the POST macro
//                                 
// 
// USEPREFIX statement: Defines a format string prefix to be used when logging the event, 
// below the STDPREFIX is used. The first value is the trace function name with out parenthesis
// and the second value is the format string to be used.
// 
// USESUFFIX statement: Defines a suffix format string that gets logged with the event. 
// 
// FUNC statement: Defines the name and signature of the trace function. The function defined 
// below takes one argument, no format string, and predefines the flag equal to FLAG_ONE.
//
//
//begin_wpp config
//USEPREFIX (TRACE_RETURN, "%!STDPREFIX!");
//FUNC TRACE_RETURN{FLAG=FLAG_ONE}(EXP);
//USESUFFIX (TRACE_RETURN, "Function Return=%!STATUS!",EXP);
//end_wpp

//
// PRE macro: The name of the macro includes the condition arguments FLAGS and EXP
//            define in FUNC above
//
#define WPP_FLAG_EXP_PRE(FLAGS, HR) {if (HR != STATUS_SUCCESS) {

//
// POST macro
// The name of the macro includes the condition arguments FLAGS and EXP
//            define in FUNC above
#define WPP_FLAG_EXP_POST(FLAGS, HR) ;}}

// 
// The two macros below are for checking if the event should be logged and for 
// choosing the logger handle to use when calling the ETW trace API
//
#define WPP_FLAG_EXP_ENABLED(FLAGS, HR) WPP_FLAG_ENABLED(FLAGS)
#define WPP_FLAG_EXP_LOGGER(FLAGS, HR) WPP_FLAG_LOGGER(FLAGS)

#endif