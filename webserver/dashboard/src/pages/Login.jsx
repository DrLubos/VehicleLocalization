import React, { useState, useEffect } from "react";
import {
  Box,
  Button,
  FormControl,
  FormLabel,
  Input,
  Heading,
  VStack,
  useColorModeValue,
  Divider,
} from "@chakra-ui/react";
import { useNavigate } from "react-router-dom";
import RegistrationModal from "../components/RegistrationModal";

function Login({ onLogin }) {
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [isRegOpen, setIsRegOpen] = useState(false);
  const navigate = useNavigate();
  const bg = useColorModeValue("white", "gray.700");

  useEffect(() => {
    const token = localStorage.getItem("authToken");
    if (token) {
      navigate("/", { replace: true });
    }
  }, [navigate]);

  const handleLogin = async (e) => {
    e.preventDefault();
    try {
      const response = await fetch("https://webapi.vehiclemap.xyz/login", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ username, password }),
      });
      if (!response.ok) {
        throw new Error(`Login failed with status ${response.status}`);
      }
      const data = await response.json();
      localStorage.setItem("authToken", data.token);
      localStorage.setItem("username", data.username);
      if (onLogin) {
        onLogin();
      }
      navigate("/", { replace: true });
    } catch (error) {
      console.error("Login failed:", error);
      alert("Login failed, please check your credentials.");
    }
  };

  const handleRegister = async ({ username, email, password }) => {
    try {
      const response = await fetch("https://webapi.vehiclemap.xyz/register", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ username, email, password }),
      });
      if (!response.ok) {
        throw new Error(`Registration failed with status ${response.status}`);
      }
      const data = await response.json();
      alert("Registration successful. Please login.");
      setIsRegOpen(false);
    } catch (error) {
      console.error("Registration failed:", error);
      alert("Registration failed. " + error.message);
    }
  };

  return (
    <Box
      minH="100vh"
      display="flex"
      justifyContent="center"
      alignItems="center"
      bg={useColorModeValue("gray.50", "gray.800")}
    >
      <Box w="sm" p={6} borderWidth="1px" borderRadius="lg" bg={bg} shadow="lg">
        <Heading mb={6} textAlign="center">
          Login to VehicleMap
        </Heading>
        <form onSubmit={handleLogin}>
          <VStack spacing={4}>
            <FormControl id="username" isRequired>
              <FormLabel>Username</FormLabel>
              <Input
                type="text"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
              />
            </FormControl>
            <FormControl id="password" isRequired>
              <FormLabel>Password</FormLabel>
              <Input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
              />
            </FormControl>
            <Button colorScheme="blue" type="submit" width="full">
              Login
            </Button>
          </VStack>
        </form>
        <Divider my={4} />
        <Button colorScheme="teal" variand="outline" width="full" onClick={() => setIsRegOpen(true)}>
          Create New User
        </Button>
        <RegistrationModal
          isOpen={isRegOpen}
          onClose={() => setIsRegOpen(false)}
          onRegister={handleRegister}
        />
      </Box>
    </Box>
  );
}

export default Login;
