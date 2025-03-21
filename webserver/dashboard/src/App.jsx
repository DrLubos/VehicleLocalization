import React, { useState, useEffect } from "react";
import { BrowserRouter, Routes, Route, Navigate, useNavigate } from "react-router-dom";
import Login from "./pages/Login";
import Dashboard from "./pages/Dashboard";

function App() {
  const [token, setToken] = useState(localStorage.getItem("authToken") || "");
  const navigate = useNavigate();
  const logoutUser = () => {
    localStorage.removeItem("authToken");
    localStorage.removeItem("username");
    setToken("");
    navigate("/login", { replace: true });
  };
  useEffect(() => {
    const storedToken = localStorage.getItem("authToken");
    if (!storedToken) return;

    try {
      const payload = JSON.parse(atob(storedToken.split(".")[1]));
      const expTime = payload.exp * 1000;
      const delay = expTime - Date.now();
      if (delay > 0) {
        const timer = setTimeout(() => {
          logoutUser();
        }, delay);
        return () => clearTimeout(timer);
      } else {
        logoutUser();
      }
    } catch (e) {
      console.error("Token decoding error:", e);
      logoutUser();
    }
  }, [token]);
  return (
      <Routes>
        <Route
          path="/login"
          element={<Login onLogin={() => setToken(localStorage.getItem("authToken"))} />}
        />
        <Route
          path="/"
          element={token ? <Dashboard /> : <Navigate to="/login" replace />}
        />
      </Routes>
  );
}

export default App;
