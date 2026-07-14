if(NOT DEFINED AX_STYLIZED_WEB_HTML OR NOT EXISTS "${AX_STYLIZED_WEB_HTML}")
  message(FATAL_ERROR "AX_STYLIZED_WEB_HTML must name the built stylized-smoke HTML file")
endif()

if(NOT DEFINED AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES OR
   NOT AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES MATCHES "^[0-9]+$")
  message(FATAL_ERROR "AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES must be a non-negative integer")
endif()

find_program(BROTLI_EXECUTABLE NAMES brotli)
if(NOT BROTLI_EXECUTABLE)
  message(FATAL_ERROR "brotli is required for axmol_stylized_web_size_gate")
endif()

get_filename_component(_output_dir "${AX_STYLIZED_WEB_HTML}" DIRECTORY)
get_filename_component(_output_name "${AX_STYLIZED_WEB_HTML}" NAME_WE)
set(_total_size 0)

foreach(_extension IN ITEMS wasm js)
  set(_input "${_output_dir}/${_output_name}.${_extension}")
  set(_compressed "${_input}.size-gate.br")
  if(NOT EXISTS "${_input}")
    message(FATAL_ERROR "Stylized Web size input does not exist: ${_input}")
  endif()

  execute_process(
    COMMAND "${BROTLI_EXECUTABLE}" -q 11 -f -o "${_compressed}" "${_input}"
    RESULT_VARIABLE _brotli_result
    ERROR_VARIABLE _brotli_error
  )
  if(NOT _brotli_result EQUAL 0)
    file(REMOVE "${_compressed}")
    message(FATAL_ERROR "brotli failed for ${_input}: ${_brotli_error}")
  endif()

  file(SIZE "${_compressed}" _compressed_size)
  file(REMOVE "${_compressed}")
  math(EXPR _total_size "${_total_size} + ${_compressed_size}")
  message(STATUS "${_output_name}.${_extension} Brotli q11: ${_compressed_size} bytes")
endforeach()

if(_total_size GREATER AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES)
  math(EXPR _delta "${_total_size} - ${AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES}")
  message(FATAL_ERROR
    "Stylized Web size gate failed: ${_total_size} > "
    "${AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES} bytes (+${_delta})")
endif()

math(EXPR _headroom "${AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES} - ${_total_size}")
message(STATUS
  "Stylized Web size gate passed: ${_total_size} <= "
  "${AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES} bytes (${_headroom} bytes headroom)")
