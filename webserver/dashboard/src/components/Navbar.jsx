import React from "react";
import { Flex, Box, Heading, Button, IconButton, useColorMode, useColorModeValue } from "@chakra-ui/react";
import { MoonIcon, SunIcon } from "@chakra-ui/icons";
import { useNavigate } from "react-router-dom";

const Navbar = () => {
  const username = localStorage.getItem("username") || "Implement me!";
  const { colorMode, toggleColorMode } = useColorMode();
  const bg = useColorModeValue("blue.600", "gray.800");
  const textColor = useColorModeValue("white", "white");
  const hoverBg = colorMode === "light" ? "blue.700" : "gray.600";
  const navigate = useNavigate();

  const handleLogout = () => {
    localStorage.removeItem("authToken");
    navigate("/login");
  };

  return (
    <Flex bg={bg} p={4} align="center">
      <Box>
        <Heading size="sm" color={textColor}>
          {username}
        </Heading>
      </Box>
      <Box flex="1" textAlign="center">
        <Heading size="lg" color={textColor} cursor="pointer" onClick={() => navigate("/")}>
          VehicleMap
        </Heading>
      </Box>
      <Box>
        <IconButton
          aria-label="Toggle dark/light mode"
          icon={colorMode === "light" ? <MoonIcon /> : <SunIcon />}
          onClick={toggleColorMode}
          variant="ghost"
          color={textColor}
          mr={2}
          _hover={{ bg: hoverBg }}
        />
        {username && (
          <>
            <Button variant="ghost" color={textColor} mr={2} onClick={() => navigate("/settings")} _hover={{ bg: hoverBg }}>
              Settings
            </Button>
            <Button variant="ghost" color={textColor} onClick={handleLogout} _hover={{ bg: hoverBg }}>
              Logout
            </Button>
          </>
        )}
      </Box>
    </Flex>
  );
};

export default Navbar;
