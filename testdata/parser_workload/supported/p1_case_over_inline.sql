SELECT sum(x) OVER (PARTITION BY k ORDER BY ts) AS w, CASE a WHEN 1 THEN 'one' ELSE 'other' END AS label FROM t
