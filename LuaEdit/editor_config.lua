-- Lua Code Editor Colours
do
  -- Definitions for the style option
	local regular, bold, italic, underline, hidden = 0, 1, 2, 4, 8
	-- Commonly used colours (all colours come from the wxWidgets palette)
	local background = "WHITE"
	local comment    = "FOREST GREEN"
	-- Colour definitions  {forground    , background, style  }
  colours = {
	  ["Default"       ] = {"BLACK"      , background, regular},
	  ["Comment"       ] = {comment      , background, regular},
	  ["Comment line"  ] = {comment      , background, regular},
	  ["Comment (Doc)" ] = {comment      , background, regular},
	  ["Number"        ] = {"CORAL"      , background, regular},
	  ["String"        ] = {"GREY"       , background, regular},
	  ["Character"     ] = {"GREY"       , background, regular},
	  ["Literal String"] = {"PURPLE"     , background, regular},
    ["String (EOL)"  ] = {"PURPLE"     , background, regular},
	  ["Preprocessor"  ] = {"GREY"       , background, regular},
	  ["Operator"      ] = {"NAVY"       , background, bold   },
	  ["Identifier"    ] = {"BLACK"      , background, regular},
	  ["Keyword1"      ] = {"BLUE"       , background, bold   },
	  ["Keyword2"      ] = {"MEDIUM BLUE", background, bold   },
    ["Keyword3"      ] = {"FIREBRICK"  , background, regular},
	  ["Keyword4"      ] = {"TAN"        , background, regular},
	  ["Keyword5"      ] = {"DARK GREY"  , background, regular},
	  ["Keyword6"      ] = {"GREY"       , background, regular},
	  ["Keyword7"      ] = {"GREY"       , background, regular},
	  ["Keyword8"      ] = {"GREY"       , background, regular},
  }
end

-- Lua Code Editor Keywords
keywords = {
  [1] = "true false nil",
  [2] = "and break do else elseif end false for function if in local nil not or repeat return then true until while _G",
  [3] = "Reference Inherit GameData",
  [4] = "InheritMeta MetaData",
}
